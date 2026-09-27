// pwpr.cpp  — Piecewise Polynomial Regression (deg 1..4), binary f32 I/O,
//              parallel per-segment degree search, and standalone decompressor
// Build: g++ -std=c++17 -O2 pwpr.cpp -o pwpr
// Usage:
//   Compress:    ./pwpr -i input.f32 -z segments.bin -o recon.f32 -t <tol> [-1] [-2] [-3] [-4]
//   Decompress:  ./pwpr -d -z segments.bin -o recon.f32
//
// Compressed record (little-endian on LE hosts):
//   uint32 rec_size_bytes   // includes this field
//   uint64 start (inclusive)
//   uint64 end   (exclusive)
//   uint32 degree (1..4)
//   float  coeff[5]         // local poly a0..a4 for x'=(i-start)/(len-1) (or 0 if len==1); unused tail = 0

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <limits>
#include <getopt.h>

static constexpr int N_MAX = 4;
static constexpr int COEFF_CAP = N_MAX + 1;

// ---------- helpers ----------
static std::uintmax_t fileSize(const std::string& filename) {
    return std::filesystem::file_size(filename);
}

static std::string humanSize(std::uintmax_t bytes) {
    const double KB = 1000.0, MB = 1000.0 * 1000.0;
    std::ostringstream oss; oss << std::fixed << std::setprecision(2);
    if (bytes < (std::uintmax_t)MB) oss << (bytes/KB) << " KB"; else oss << (bytes/MB) << " MB";
    return oss.str();
}

static std::vector<float> readBinaryFloats(const std::string& filename) {
    std::ifstream fin(filename, std::ios::binary | std::ios::ate);
    if (!fin) throw std::runtime_error("Cannot open input: " + filename);
    auto sz = fin.tellg();
    if (sz < 0) throw std::runtime_error("tellg() failed: " + filename);
    if (sz % sizeof(float)) throw std::runtime_error("File size not multiple of float: " + filename);
    std::size_t n = (std::size_t)sz / sizeof(float);
    std::vector<float> data(n);
    fin.seekg(0, std::ios::beg);
    fin.read(reinterpret_cast<char*>(data.data()), n*sizeof(float));
    return data;
}

static void writeBinaryFloats(const std::string& filename, const std::vector<float>& data) {
    std::ofstream fout(filename, std::ios::binary);
    if (!fout) throw std::runtime_error("Cannot open for write: " + filename);
    fout.write(reinterpret_cast<const char*>(data.data()), data.size()*sizeof(float));
}
// ---------- tiny linear algebra ----------
static std::vector<double> solve_linear_system(std::vector<std::vector<double>> A, std::vector<double> b) {
    int m = (int)A.size();
    for (int i = 0; i < m; ++i) A[i].push_back(b[i]);

    for (int col = 0; col < m; ++col) {
        int piv = col;
        for (int r = col+1; r < m; ++r)
            if (std::fabs(A[r][col]) > std::fabs(A[piv][col])) piv = r;

        if (std::fabs(A[piv][col]) < 1e-14) return std::vector<double>(m, 0.0);

        if (piv != col) std::swap(A[piv], A[col]);

        double div = A[col][col];
        for (int c = col; c <= m; ++c) A[col][c] /= div;

        for (int r = 0; r < m; ++r) if (r != col) {
            double f = A[r][col];
            for (int c = col; c <= m; ++c) A[r][c] -= f * A[col][c];
        }
    }

    std::vector<double> x(m);
    for (int i = 0; i < m; ++i) x[i] = A[i][m];
    return x;
}

// fit degree 'deg' on [l,r) with normalized local coordinate x' = k/(len-1) (or 0 if len==1)
static std::vector<double> fit_poly_local(const std::vector<float>& y, std::size_t l, std::size_t r, int deg) {
    const int m = deg + 1;
    std::size_t len = r - l;
    if (len == 0) return std::vector<double>(m, 0.0);

    // FP-aware normalization: k -> k / (len-1) in [0,1]
    double denom = (len > 1) ? (double)(len - 1) : 1.0;

    std::vector<std::vector<double>> XtX(m, std::vector<double>(m, 0.0));
    std::vector<double> Xty(m, 0.0);

    for (std::size_t k = 0; k < len; ++k) {
        double xv = (double)k / denom;  // normalized x in [0,1] (or 0 if len==1)

        std::vector<double> px(m, 1.0);
        for (int p = 1; p < m; ++p) px[p] = px[p-1] * xv;

        double yi = (double)y[l + k];
        for (int a = 0; a < m; ++a) {
            Xty[a] += px[a] * yi;
            for (int b = 0; b < m; ++b) XtX[a][b] += px[a] * px[b];
        }
    }
    return solve_linear_system(XtX, Xty);
}

static inline double eval_poly_local(const std::vector<double>& a, double xprime) {
    double s = 0.0, p = 1.0;
    for (double c : a) { s += c * p; p *= xprime; }
    return s;
}

struct FitResult {
    bool ok = false;
    std::size_t start = 0, end = 0; // [start,end)
    int degree = 0;
    std::vector<double> coeff;
    double sse = std::numeric_limits<double>::infinity();
};

static bool segment_ok(const std::vector<float>& y, std::size_t l, std::size_t r, int deg, double tol,
                       std::vector<double>& coeff_out, double& sse_out, double& maxae_out)
{
    std::size_t len = r - l;
    if (len < (std::size_t)(deg + 1)) return false;

    auto a = fit_poly_local(y, l, r, deg);

    double denom = (len > 1) ? (double)(len - 1) : 1.0;

    double sse = 0.0, maxae = 0.0;
    for (std::size_t i = l; i < r; ++i) {
        double xlocal = (double)(i - l);
        double xprime = xlocal / denom;  // normalized [0,1] (or 0 if len==1)

        double yh = eval_poly_local(a, xprime);
        double e = std::fabs((double)y[i] - yh);
        if (e > maxae) maxae = e;
        sse += e * e;

        if (maxae > tol) {
            coeff_out = std::move(a);
            sse_out = sse;
            maxae_out = maxae;
            return false;
        }
    }
    coeff_out = std::move(a);
    sse_out = sse;
    maxae_out = maxae;
    return true;
}

static std::size_t find_max_end_under_tol(const std::vector<float>& y, std::size_t l, int deg, double tol,
                                          std::vector<double>& best_coeff, double& best_sse)
{
    const std::size_t n = y.size();
    std::size_t low  = l + (std::size_t)(deg + 1);
    if (low > n) return l;
    std::size_t high = n, best_r = l;
    std::vector<double> coeff; double sse = 0, mae = 0;

    while (low <= high) {
        std::size_t mid = (low + high) >> 1;
        std::vector<double> c; double s = 0, m = 0;
        bool ok = segment_ok(y, l, mid, deg, tol, c, s, m);
        if (ok) {
            best_r = mid;
            coeff = std::move(c);
            sse = s;
            low = mid + 1;
        } else {
            if (mid == 0) break;
            high = mid - 1;
        }
    }
    if (best_r > l) { best_coeff = std::move(coeff); best_sse = sse; }
    return best_r;
}

struct Segment {
    std::size_t start;                 // inclusive
    std::size_t end;                   // exclusive
    int degree;                        // 1..4
    std::array<float, COEFF_CAP> coeff;// local a0..a4
    Segment() : start(0), end(0), degree(1) { coeff.fill(0.0f); }
    Segment(std::size_t s, std::size_t e, int d, const std::vector<double>& a)
        : start(s), end(e), degree(d) {
        coeff.fill(0.0f);
        for (int j=0; j<=d && j<COEFF_CAP; ++j) coeff[j] = (float)a[j];
    }
};

static std::vector<Segment> pwprCompress(const std::vector<float>& y, double tol,
                                         const std::array<bool, N_MAX+1>& use_deg)
{
    const std::size_t n = y.size();
    std::vector<Segment> segs;
    if (!n) return segs;

    std::size_t s = 0;
    while (s < n) {
        std::vector<std::future<FitResult>> futs;
        for (int d=1; d<=N_MAX; ++d) if (use_deg[d]) {
            futs.push_back(std::async(std::launch::async, [&, d]() -> FitResult {
                FitResult fr; fr.start = s; fr.degree = d;
                if (n - s < (std::size_t)(d + 1)) return fr;
                std::vector<double> coeff; double sse = 0;
                std::size_t r = find_max_end_under_tol(y, s, d, tol, coeff, sse);
                if (r > s) {
                    fr.ok = true; fr.end = r; fr.coeff = std::move(coeff); fr.sse = sse;
                }
                return fr;
            }));
        }

        FitResult best; std::size_t best_cov = 0; bool have = false;
        for (auto& f : futs) {
            FitResult fr = f.get();
            if (!fr.ok) continue;
            std::size_t cov = fr.end - fr.start;
            if (!have || cov > best_cov ||
               (cov == best_cov && (fr.sse < best.sse - 1e-12 ||
               (std::fabs(fr.sse - best.sse) <= 1e-12 && fr.degree < best.degree)))) {
                best = std::move(fr); best_cov = cov; have = true;
            }
        }

        if (!have) {
            // force minimal window of smallest allowed degree (or 1 sample)
            int chosen = -1;
            for (int d=1; d<=N_MAX; ++d)
                if (use_deg[d] && n - s >= (std::size_t)(d+1)) { chosen = d; break; }

            if (chosen < 0) {
                std::vector<double> a = { (double)y[s] };
                segs.emplace_back(s, s+1, 1, a);
                ++s;
                continue;
            }
            std::size_t r = s + (std::size_t)(chosen + 1);
            auto a = fit_poly_local(y, s, r, chosen);
            std::cerr << "[WARN] tol too strict @ " << s << "; minimal deg " << chosen
                      << " with " << (r-s) << " points\n";
            segs.emplace_back(s, r, chosen, a);
            s = r;
        } else {
            segs.emplace_back(best.start, best.end, best.degree, best.coeff);
            s = best.end;
        }
    }
    return segs;
}
// reconstruct from in-memory segments
static std::vector<float> pwprDecompress(const std::vector<Segment>& segs) {
    std::size_t n = 0;
    for (const auto& s : segs) n = std::max(n, s.end);
    std::vector<float> yhat(n, 0.0f);

    for (const auto& s : segs) {
        std::size_t len = s.end - s.start;
        double denom = (len > 1) ? (double)(len - 1) : 1.0;

        for (std::size_t i = s.start; i < s.end; ++i) {
            double xlocal = (double)(i - s.start);
            double xprime = xlocal / denom;  // normalized [0,1] (or 0 if len==1)

            double p = 1.0, sum = 0.0;
            for (int j = 0; j < COEFF_CAP; ++j) {
                sum += (double)s.coeff[j] * p;
                p *= xprime;
            }
            yhat[i] = (float)sum;
        }
    }
    return yhat;
}

// ---------- compressed file I/O ----------
static void writeSegmentsBinary(const std::string& filename, const std::vector<Segment>& segs) {
    std::ofstream fout(filename, std::ios::binary);
    if (!fout) throw std::runtime_error("Cannot open for writing: " + filename);
    const std::uint32_t rec_size = (std::uint32_t)(8 + 8 + 4 + 4*COEFF_CAP);
    for (const auto& s : segs) {
        std::uint32_t sz = rec_size;
        std::uint64_t st = (std::uint64_t)s.start;
        std::uint64_t en = (std::uint64_t)s.end;
        std::uint32_t dg = (std::uint32_t)s.degree;
        fout.write(reinterpret_cast<const char*>(&sz), sizeof(sz));
        fout.write(reinterpret_cast<const char*>(&st), sizeof(st));
        fout.write(reinterpret_cast<const char*>(&en), sizeof(en));
        fout.write(reinterpret_cast<const char*>(&dg), sizeof(dg));
        fout.write(reinterpret_cast<const char*>(s.coeff.data()), COEFF_CAP*sizeof(float));
    }
}

// NEW: read segments back from binary file (until EOF)
static std::vector<Segment> readSegmentsBinary(const std::string& filename) {
    std::ifstream fin(filename, std::ios::binary);
    if (!fin) throw std::runtime_error("Cannot open segments: " + filename);
    std::vector<Segment> segs;
    while (true) {
        std::uint32_t rec_size;
        if (!fin.read(reinterpret_cast<char*>(&rec_size), sizeof(rec_size))) break; // EOF ok

        std::uint64_t st=0,en=0; std::uint32_t dg=0;
        if (!fin.read(reinterpret_cast<char*>(&st), sizeof(st))) throw std::runtime_error("Truncated segments (start)");
        if (!fin.read(reinterpret_cast<char*>(&en), sizeof(en))) throw std::runtime_error("Truncated segments (end)");
        if (!fin.read(reinterpret_cast<char*>(&dg), sizeof(dg))) throw std::runtime_error("Truncated segments (degree)");

        Segment s;
        s.start = (std::size_t)st;
        s.end   = (std::size_t)en;
        s.degree= (int)dg;
        s.coeff.fill(0.0f);

        if (!fin.read(reinterpret_cast<char*>(s.coeff.data()), COEFF_CAP*sizeof(float)))
            throw std::runtime_error("Truncated segments (coeff)");

        // optional: skip any padding if rec_size is larger than expected
        std::uint32_t expected = (std::uint32_t)(8 + 8 + 4 + 4*COEFF_CAP);
        if (rec_size > expected) {
            fin.seekg(rec_size - expected, std::ios::cur);
            if (!fin) throw std::runtime_error("Bad rec_size in segment file");
        }
        segs.push_back(std::move(s));
    }
    return segs;
}

static void writeSegmentsText(const std::string& filename, const std::vector<Segment>& segs) {
    std::ofstream fout(filename);
    if (!fout) throw std::runtime_error("Cannot open for writing: " + filename);
    fout << std::fixed << std::setprecision(6);
    fout << "# start,end,degree,a0,a1,a2,a3,a4\n";
    for (const auto& s : segs) {
        fout << s.start << "," << s.end << "," << s.degree;
        for (int j=0; j<COEFF_CAP; ++j) fout << "," << s.coeff[j];
        fout << "\n";
    }
}

// ---------- main ----------
int main(int argc, char* argv[]) {
    try {
        std::string inFile  = "input_data.f32";
        std::string segFile = "segments.bin";
        std::string outFile = "recon_output.f32";
        double tol = 0.5;

        std::array<bool, N_MAX+1> use_deg{}; for (int d=1; d<=N_MAX; ++d) use_deg[d]=true;
        bool degree_filter_set = false;
        bool decompress_only = false;

        int opt;
        // add -d for decompress-only, -t for tolerance, -1..-4 for degree filter
        while ((opt = getopt(argc, argv, "di:z:o:e:01234")) != -1) {
            switch (opt) {
                case 'd': decompress_only = true; break;
                case 'i': inFile = optarg; break;
                case 'z': segFile = optarg; break;
                case 'o': outFile = optarg; break;
                case 'e': tol = std::stod(optarg); break;
                case '0': break;
                case '1': if (!degree_filter_set) { for (int d=1; d<=N_MAX; ++d) use_deg[d]=false; degree_filter_set=true; } use_deg[1]=true; break;
                case '2': if (!degree_filter_set) { for (int d=1; d<=N_MAX; ++d) use_deg[d]=false; degree_filter_set=true; } use_deg[2]=true; break;
                case '3': if (!degree_filter_set) { for (int d=1; d<=N_MAX; ++d) use_deg[d]=false; degree_filter_set=true; } use_deg[3]=true; break;
                case '4': if (!degree_filter_set) { for (int d=1; d<=N_MAX; ++d) use_deg[d]=false; degree_filter_set=true; } use_deg[4]=true; break;
                default:
                    std::cerr << "Usage:\n"
                              << "  Compress:   " << argv[0] << " -i input.f32 -z segments.bin -o recon.f32 -t <tol> [-1] [-2] [-3] [-4]\n"
                              << "  Decompress: " << argv[0] << " -d -z segments.bin -o recon.f32\n";
                    return 1;
            }
        }

        if (decompress_only) {
            // ---------- Decompress-only ----------
            std::cout << "Mode: Decompress-only\n";
            std::cout << "Segments file:     " << segFile << "\n";
            std::cout << "Reconstructed out: " << outFile << "\n";

            auto segs = readSegmentsBinary(segFile);
            auto yrec = pwprDecompress(segs);
            writeBinaryFloats(outFile, yrec);
            std::cout << "Wrote reconstructed samples to '" << outFile
                      << "' (" << humanSize(fileSize(outFile)) << ")\n";
            return 0;
        }

        // ---------- Compress (+ reconstruct) ----------
        std::cout << "Mode: Compress\n";
        std::cout << "Input file:        " << inFile  << "\n";
        std::cout << "Segments file:     " << segFile << "\n";
        std::cout << "Reconstructed out: " << outFile << "\n";
        std::cout << "Epsilon (e):       " << tol     << "\n";
        std::cout << "Degrees enabled:   "; for (int d=1; d<=N_MAX; ++d) if (use_deg[d]) std::cout << d << " "; std::cout << "\n";

        auto y = readBinaryFloats(inFile);
        const std::uintmax_t rawBytes = y.size()*sizeof(float);
        std::cout << "Read " << y.size() << " samples (" << humanSize(rawBytes) << ")\n";

        const auto t0 = std::chrono::steady_clock::now();
        auto segs = pwprCompress(y, tol, use_deg);
        const auto t1 = std::chrono::steady_clock::now();
        auto compress_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        std::cout << "Compression produced " << segs.size() << " segments in " << compress_ms << " ms\n";

        // write segments (bin + txt)
        // const std::string segTxt = segFile + ".txt";
        writeSegmentsBinary(segFile, segs);
        // writeSegmentsText(segTxt, segs);
        std::cout << "Wrote segments to '" << segFile << "' (" << humanSize(fileSize(segFile)) << ")\n";

        int count[5]={0,0,0,0,0};

        for(const auto& seg : segs) {
            count[0]++;

            if (seg.degree==1) count[1]++;
            if (seg.degree==2) count[2]++;
            if (seg.degree==3) count[3]++;
            if (seg.degree==4) count[4]++;
        }

        std::cout << "\n\nSegment counts total: " << count[0] << "\n";


        if (count[0] == 0) {
            std::cout << "No segments.\n";
        } else {
            for (int d = 1; d <= 4; ++d) {
                double pct = (static_cast<double>(count[d]) /
                static_cast<double>(count[0])) * 100.0;

                std::cout << "- Segment degree " << d << " count: "
                << count[d]
                << "  percentile: " << std::fixed << std::setprecision(3)
                << pct << "%\n";
            }
        }


        //std::cout << "Also wrote human-readable segments to '" << segTxt << "'\n";

        // reconstruct
        const auto t2 = std::chrono::steady_clock::now();
        auto yrec = pwprDecompress(segs);
        const auto t3 = std::chrono::steady_clock::now();
        auto decompress_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();
        std::cout << "Decompression rebuilt " << yrec.size() << " samples in " << decompress_ms << " ms\n";

        writeBinaryFloats(outFile, yrec);
        std::cout << "Wrote reconstructed samples to '" << outFile
                  << "' (" << humanSize(fileSize(outFile)) << ")\n";

        // quick MAE if same length
        if (yrec.size() == y.size()) {
            double mae = 0.0;
            for (std::size_t i=0; i<y.size(); ++i) mae += std::fabs((double)yrec[i] - (double)y[i]);
            mae /= (double)y.size();
            std::cout << "MAE vs original: " << std::setprecision(6) << mae << "\n";
        } else {
            std::cout << "Note: reconstructed length differs; MAE skipped.\n";
        }
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

// pwpr.cpp  — PFPL-style Piecewise Polynomial Regression (deg 1..4), binary f32 I/O,
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
//
// Internally, this version uses:
//   - x'-normalization (index mapped to [0,1])
//   - y-centering & scaling
//   - Chebyshev basis for regression (internal only)
//   - residual correction (second fit, same degree)
//   - FP-aware tolerance scaling

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
static constexpr double FP_REL_EPS = 1e-6;  // for FP-aware tolerance scaling

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

        if (std::fabs(A[piv][col]) < 1e-14)
            return std::vector<double>(m, 0.0);

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

// ---------- Chebyshev helpers (on x' in [0,1]) ----------

// Build Chebyshev T_k(x') monomial expansions up to degree 'deg'.
// T_0(x) = 1
// T_1(x) = x
// T_{k}(x) = 2x T_{k-1}(x) - T_{k-2}(x)
static std::vector<std::vector<double>> build_chebyshev_to_monomial(int deg) {
    int m = deg + 1;
    std::vector<std::vector<double>> Tmono(m, std::vector<double>(m, 0.0));

    // T0(x) = 1
    Tmono[0][0] = 1.0;

    if (deg >= 1) {
        // T1(x) = x
        Tmono[1][1] = 1.0;
    }

    for (int k = 2; k <= deg; ++k) {
        std::vector<double> next(m, 0.0);
        // 2x * T_{k-1}(x)
        const auto& prev = Tmono[k-1];
        for (int j = 1; j < m; ++j) {
            next[j] += 2.0 * prev[j-1];  // shift degree up by 1
        }
        // - T_{k-2}(x)
        const auto& prev2 = Tmono[k-2];
        for (int j = 0; j < m; ++j) {
            next[j] -= prev2[j];
        }
        Tmono[k] = std::move(next);
    }
    return Tmono;
}

// Fit Chebyshev basis (on x' in [0,1]) to y_norm[k], return monomial coeffs in x'.
static std::vector<double> fit_cheb_to_normed(const std::vector<double>& y_norm,
                                              std::size_t len, int deg)
{
    const int m = deg + 1;
    if (len == 0) return std::vector<double>(m, 0.0);

    double denom = (len > 1) ? (double)(len - 1) : 1.0;

    // Build normal equations in Chebyshev coordinate space.
    std::vector<std::vector<double>> XtX(m, std::vector<double>(m, 0.0));
    std::vector<double> Xty(m, 0.0);

    for (std::size_t k = 0; k < len; ++k) {
        double xprime = (double)k / denom;  // in [0,1]
        // Chebyshev basis at xprime:
        std::vector<double> T(m, 0.0);
        T[0] = 1.0;
        if (deg >= 1) T[1] = xprime;
        for (int p = 2; p <= deg; ++p)
            T[p] = 2.0 * xprime * T[p-1] - T[p-2];

        double yi = y_norm[k];
        for (int a = 0; a < m; ++a) {
            Xty[a] += T[a] * yi;
            for (int b = 0; b < m; ++b)
                XtX[a][b] += T[a] * T[b];
        }
    }

    // Solve for Chebyshev coefficients
    std::vector<double> c_cheb = solve_linear_system(XtX, Xty);

    // Convert Chebyshev coefficients to monomial coefficients in x'.
    auto Tmono = build_chebyshev_to_monomial(deg);
    std::vector<double> a_mono(m, 0.0);
    for (int k = 0; k <= deg; ++k) {
        for (int j = 0; j <= deg; ++j) {
            a_mono[j] += c_cheb[k] * Tmono[k][j];
        }
    }
    return a_mono;
}

// Center and scale y in [l,r), produce y_norm[k], mu, sigma.
static void center_and_scale_segment(const std::vector<float>& y,
                                     std::size_t l, std::size_t r,
                                     std::vector<double>& y_norm,
                                     double& mu, double& sigma)
{
    std::size_t len = r - l;
    y_norm.assign(len, 0.0);

    if (len == 0) {
        mu = 0.0;
        sigma = 1.0;
        return;
    }

    // mean
    double sum = 0.0;
    for (std::size_t k = 0; k < len; ++k)
        sum += (double)y[l + k];
    mu = sum / (double)len;

    // scale: max deviation from mean
    double maxdev = 0.0;
    for (std::size_t k = 0; k < len; ++k) {
        double d = std::fabs((double)y[l + k] - mu);
        if (d > maxdev) maxdev = d;
    }
    sigma = (maxdev > 0.0) ? maxdev : 1.0;

    for (std::size_t k = 0; k < len; ++k) {
        y_norm[k] = ((double)y[l + k] - mu) / sigma;
    }
}

// PFPL-style fit: y-centering/scaling + Chebyshev + residual correction.
// Returns monomial coefficients a_total[0..deg] in x'.
static std::vector<double> fit_poly_local(const std::vector<float>& y,
                                          std::size_t l, std::size_t r, int deg)
{
    const int m = deg + 1;
    std::size_t len = r - l;
    if (len == 0) return std::vector<double>(m, 0.0);

    // ----- MAIN FIT -----
    std::vector<double> y_norm;
    double mu = 0.0, sigma = 1.0;
    center_and_scale_segment(y, l, r, y_norm, mu, sigma);

    // Fit Chebyshev on normalized y
    std::vector<double> a_main_norm = fit_cheb_to_normed(y_norm, len, deg);

    // Map back to original y-scale: y ≈ mu + sigma * P_norm(x')
    std::vector<double> a_main(m, 0.0);
    for (int j = 0; j < m; ++j)
        a_main[j] = sigma * a_main_norm[j];
    a_main[0] += mu;

    // ----- RESIDUAL CORRECTION (same degree) -----
    std::vector<double> residual(len, 0.0);
    double denom = (len > 1) ? (double)(len - 1) : 1.0;

    for (std::size_t k = 0; k < len; ++k) {
        double xprime = (double)k / denom;
        double p = 1.0, yh = 0.0;
        for (int j = 0; j < m; ++j) {
            yh += a_main[j] * p;
            p *= xprime;
        }
        residual[k] = (double)y[l + k] - yh;
    }

    // Center/scale residuals
    std::vector<double> r_norm;
    double mu_r = 0.0, sigma_r = 1.0;
    {
        // reuse center_and_scale logic on residual
        std::size_t len_r = len;
        r_norm.assign(len_r, 0.0);
        if (len_r > 0) {
            double sum_r = 0.0;
            for (std::size_t k = 0; k < len_r; ++k) sum_r += residual[k];
            mu_r = sum_r / (double)len_r;

            double maxdev_r = 0.0;
            for (std::size_t k = 0; k < len_r; ++k) {
                double d = std::fabs(residual[k] - mu_r);
                if (d > maxdev_r) maxdev_r = d;
            }
            sigma_r = (maxdev_r > 0.0) ? maxdev_r : 1.0;

            for (std::size_t k = 0; k < len_r; ++k) {
                r_norm[k] = (residual[k] - mu_r) / sigma_r;
            }
        }
    }

    std::vector<double> a_res_norm = fit_cheb_to_normed(r_norm, len, deg);

    std::vector<double> a_res(m, 0.0);
    for (int j = 0; j < m; ++j)
        a_res[j] = sigma_r * a_res_norm[j];
    a_res[0] += mu_r;

    // Final polynomial: a_total = a_main + a_res
    std::vector<double> a_total(m, 0.0);
    for (int j = 0; j < m; ++j)
        a_total[j] = a_main[j] + a_res[j];

    return a_total;
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

        // FP-aware tolerance scaling
        double tol_i = tol + FP_REL_EPS * std::fabs((double)y[i]);
        if (e > tol_i) {
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
    const std::uint32_t rec_size = (std::uint32_t)(4 + 8 + 8 + 4 + 4*COEFF_CAP);
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

// read segments back from binary file (until EOF)
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
        std::uint32_t expected = (std::uint32_t)(4 + 8 + 8 + 4 + 4*COEFF_CAP);
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
        while ((opt = getopt(argc, argv, "di:z:o:t:1234")) != -1) {
            switch (opt) {
                case 'd': decompress_only = true; break;
                case 'i': inFile = optarg; break;
                case 'z': segFile = optarg; break;
                case 'o': outFile = optarg; break;
                case 't': tol = std::stod(optarg); break;
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
        std::cout << "Tolerance (t):     " << tol     << "\n";
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
        const std::string segTxt = segFile + ".txt";
        writeSegmentsBinary(segFile, segs);
        writeSegmentsText(segTxt, segs);
        std::cout << "Wrote segments to '" << segFile << "' (" << humanSize(fileSize(segFile)) << ")\n";
        std::cout << "Also wrote human-readable segments to '" << segTxt << "'\n";

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

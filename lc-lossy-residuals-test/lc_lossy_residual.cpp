#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

// These are the four LC standalone programs, compiled with their main functions
// renamed.  See README.md for the exact generation and build commands.
int lc_lossy_compress_main(int argc, char* argv[]);
int lc_lossy_decompress_main(int argc, char* argv[]);
int lc_lossless_compress_main(int argc, char* argv[]);

namespace fs = std::filesystem;

namespace {

std::vector<std::uint8_t> read_file(const fs::path& path)
{
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) throw std::runtime_error("cannot open " + path.string());

  const std::streamsize size = input.tellg();
  if (size <= 0) throw std::runtime_error("input file is empty");

  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  input.seekg(0);
  if (!input.read(reinterpret_cast<char*>(bytes.data()), size)) {
    throw std::runtime_error("cannot read " + path.string());
  }
  return bytes;
}

void write_file(const fs::path& path, const void* data, std::size_t size)
{
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output ||
      !output.write(static_cast<const char*>(data),
                    static_cast<std::streamsize>(size))) {
    throw std::runtime_error("cannot write " + path.string());
  }
}

int call_lc(int (*entry)(int, char**), const fs::path& input,
            const fs::path& output)
{
  // LC accepts an optional fourth argument.  Supplying a null terminator makes
  // the argv array safe for the generated program's argv[3] check.
  std::string program = "embedded-lc";
  std::string in = input.string();
  std::string out = output.string();
  char* argv[] = {program.data(), in.data(), out.data(), nullptr};
  return entry(3, argv);
}

template <class Float>
void make_residual(const fs::path& original_path,
                   const fs::path& approximation_path,
                   const fs::path& residual_path)
{
  static_assert(std::is_same_v<Float, float> || std::is_same_v<Float, double>);

  const auto original_bytes = read_file(original_path);
  const auto approximation_bytes = read_file(approximation_path);

  if (original_bytes.size() != approximation_bytes.size()) {
    throw std::runtime_error("LC decompressed to a different byte count");
  }
  if (original_bytes.size() % sizeof(Float) != 0) {
    throw std::runtime_error("input size is not a multiple of the selected type");
  }

  const std::size_t count = original_bytes.size() / sizeof(Float);
  std::vector<Float> residual(count);
  long double sum_squared_error = 0.0L;
  long double max_absolute_error = 0.0L;
  std::size_t finite_errors = 0;
  std::size_t bitwise_changes = 0;

  for (std::size_t i = 0; i < count; ++i) {
    Float original;
    Float approximation;
    std::memcpy(&original, original_bytes.data() + i * sizeof(Float),
                sizeof(Float));
    std::memcpy(&approximation,
                approximation_bytes.data() + i * sizeof(Float), sizeof(Float));

    const bool identical =
        std::memcmp(original_bytes.data() + i * sizeof(Float),
                    approximation_bytes.data() + i * sizeof(Float),
                    sizeof(Float)) == 0;
    if (!identical) ++bitwise_changes;

    // LC passes non-quantizable values through unchanged.  The explicit zero
    // avoids turning identical infinities and NaNs into NaN residuals.
    residual[i] = identical ? Float{0} : original - approximation;

    if (std::isfinite(original) && std::isfinite(approximation)) {
      const long double error =
          static_cast<long double>(original) - approximation;
      const long double absolute_error = std::abs(error);
      max_absolute_error = std::max(max_absolute_error, absolute_error);
      sum_squared_error += error * error;
      ++finite_errors;
    }
  }

  write_file(residual_path, residual.data(), residual.size() * sizeof(Float));

  const long double rmse = finite_errors == 0
                               ? 0.0L
                               : std::sqrt(sum_squared_error / finite_errors);
  std::cout << std::setprecision(10)
            << "values: " << count << '\n'
            << "bitwise-changed values: " << bitwise_changes << '\n'
            << "maximum absolute error: " << max_absolute_error << '\n'
            << "RMSE: " << rmse << '\n';
}

std::uintmax_t file_size_or_zero(const fs::path& path)
{
  std::error_code error;
  const auto size = fs::file_size(path, error);
  return error ? 0 : size;
}

}  // namespace

int main(int argc, char* argv[])
{
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0]
              << " INPUT.raw OUTPUT_PREFIX f32|f64\n"
              << "The data type must match the LC lossy codec used at build time.\n";
    return 2;
  }

  try {
    const fs::path input = argv[1];
    const fs::path prefix = argv[2];
    const std::string type = argv[3];
    if (type != "f32" && type != "f64") {
      throw std::runtime_error("data type must be f32 or f64");
    }

    const fs::path lossy_stream = prefix.string() + ".lossy.lc";
    const fs::path approximation = prefix.string() + ".approx.raw";
    const fs::path residual_raw = prefix.string() + ".residual.tmp.raw";
    const fs::path residual_stream = prefix.string() + ".residual.lc";

    if (!prefix.parent_path().empty()) {
      fs::create_directories(prefix.parent_path());
    }

    if (call_lc(lc_lossy_compress_main, input, lossy_stream) != 0) {
      throw std::runtime_error("LC lossy compression failed");
    }
    if (call_lc(lc_lossy_decompress_main, lossy_stream, approximation) != 0) {
      throw std::runtime_error("LC lossy decompression failed");
    }

    if (type == "f32") {
      make_residual<float>(input, approximation, residual_raw);
    } else {
      make_residual<double>(input, approximation, residual_raw);
    }

    if (call_lc(lc_lossless_compress_main, residual_raw, residual_stream) != 0) {
      throw std::runtime_error("LC lossless residual compression failed");
    }

    std::error_code remove_error;
    fs::remove(residual_raw, remove_error);
    if (remove_error) {
      std::cerr << "warning: could not remove temporary residual file: "
                << remove_error.message() << '\n';
    }

    const auto input_size = file_size_or_zero(input);
    const auto lossy_size = file_size_or_zero(lossy_stream);
    const auto residual_size = file_size_or_zero(residual_stream);
    std::cout << "lossy stream: " << lossy_stream << " (" << lossy_size
              << " bytes)\n"
              << "approximation: " << approximation << '\n'
              << "lossless residual stream: " << residual_stream << " ("
              << residual_size << " bytes)\n"
              << "combined stored size: " << (lossy_size + residual_size)
              << " bytes";
    if (input_size != 0) {
      std::cout << " ("
                << static_cast<double>(lossy_size + residual_size) / input_size
                << "x input)";
    }
    std::cout << '\n';
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }

  return 0;
}

#include "../include/block_characterizer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Fit {
  std::vector<double> coefficients;
  double maxAbsError = std::numeric_limits<double>::infinity();
  double rmse = std::numeric_limits<double>::infinity();
  std::string expression;
};

std::string number(double value) {
  if (std::fabs(value) < 1.0e-14) value = 0.0;
  std::ostringstream out;
  out << std::setprecision(17) << value;
  return out.str();
}

bool solveLinearSystem(std::vector<std::vector<double>> a,
                       std::vector<double> b,
                       std::vector<double>& solution) {
  const int n = static_cast<int>(b.size());
  for (int column = 0; column < n; ++column) {
    int pivot = column;
    for (int row = column + 1; row < n; ++row) {
      if (std::fabs(a[row][column]) > std::fabs(a[pivot][column]))
        pivot = row;
    }
    if (std::fabs(a[pivot][column]) < 1.0e-12) return false;
    std::swap(a[pivot], a[column]);
    std::swap(b[pivot], b[column]);
    const double divisor = a[column][column];
    for (int j = column; j < n; ++j) a[column][j] /= divisor;
    b[column] /= divisor;
    for (int row = 0; row < n; ++row) {
      if (row == column) continue;
      const double factor = a[row][column];
      for (int j = column; j < n; ++j)
        a[row][j] -= factor * a[column][j];
      b[row] -= factor * b[column];
    }
  }
  solution = std::move(b);
  return true;
}

template <typename Basis>
Fit fitLinearBasis(const float* y, int size, int terms, Basis basis) {
  Fit result;
  if (!y || size <= 0 || terms <= 0 || size < terms) return result;
  std::vector<std::vector<double>> gram(
      terms, std::vector<double>(terms, 0.0));
  std::vector<double> rhs(terms, 0.0);
  std::vector<double> phi(terms);
  for (int i = 0; i < size; ++i) {
    basis(static_cast<double>(i), phi);
    for (int row = 0; row < terms; ++row) {
      rhs[row] += phi[row] * y[i];
      for (int column = 0; column < terms; ++column)
        gram[row][column] += phi[row] * phi[column];
    }
  }
  if (!solveLinearSystem(gram, rhs, result.coefficients)) return result;
  double squared = 0.0;
  result.maxAbsError = 0.0;
  for (int i = 0; i < size; ++i) {
    basis(static_cast<double>(i), phi);
    double predicted = 0.0;
    for (int j = 0; j < terms; ++j)
      predicted += result.coefficients[j] * phi[j];
    const double error = predicted - y[i];
    result.maxAbsError = std::max(result.maxAbsError, std::fabs(error));
    squared += error * error;
  }
  result.rmse = std::sqrt(squared / size);
  return result;
}

Fit polynomialFit(const float* y, int size, int degree) {
  Fit fit = fitLinearBasis(y, size, degree + 1,
      [degree](double x, std::vector<double>& phi) {
        phi[0] = 1.0;
        for (int j = 1; j <= degree; ++j) phi[j] = phi[j - 1] * x;
      });
  if (!std::isfinite(fit.maxAbsError)) return fit;
  std::string expression = number(fit.coefficients[0]);
  for (int degreeIndex = 1; degreeIndex <= degree; ++degreeIndex) {
    std::string term = number(fit.coefficients[degreeIndex]) + "*x";
    for (int power = 1; power < degreeIndex; ++power) term += "*x";
    expression = "(" + expression + ")+" + term;
  }
  fit.expression = expression;
  return fit;
}

std::vector<std::pair<double, double>> spectralPeaks(
    const float* y, int size, int count) {
  std::vector<std::pair<double, double>> peaks;
  if (size < 8) return peaks;
  const int maxBin = size / 2;
  for (int k = 1; k <= maxBin; ++k) {
    double real = 0.0, imaginary = 0.0;
    for (int i = 0; i < size; ++i) {
      const double angle = 2.0 * kPi * k * i / size;
      real += y[i] * std::cos(angle);
      imaginary -= y[i] * std::sin(angle);
    }
    peaks.emplace_back(real * real + imaginary * imaginary,
                       2.0 * kPi * k / size);
  }
  std::partial_sort(peaks.begin(),
                    peaks.begin() + std::min<int>(count, peaks.size()),
                    peaks.end(),
                    [](const auto& left, const auto& right) {
                      return left.first > right.first;
                    });
  if (static_cast<int>(peaks.size()) > count) peaks.resize(count);
  return peaks;
}

Fit sinusoidFit(const float* y, int size, double omega, bool trend) {
  const int terms = trend ? 4 : 3;
  Fit fit = fitLinearBasis(y, size, terms,
      [omega, trend](double x, std::vector<double>& phi) {
        phi[0] = 1.0;
        phi[1] = std::sin(omega * x);
        phi[2] = std::cos(omega * x);
        if (trend) phi[3] = x;
      });
  if (!std::isfinite(fit.maxAbsError)) return fit;
  const double amplitude = std::hypot(fit.coefficients[1], fit.coefficients[2]);
  const double phase = std::atan2(fit.coefficients[2], fit.coefficients[1]);
  fit.expression = number(fit.coefficients[0]) + "+(" +
      number(amplitude) + "*sin((" + number(omega) + "*x)+" +
      number(phase) + "))";
  if (trend)
    fit.expression += "+(" + number(fit.coefficients[3]) + "*x)";
  return fit;
}

Fit refinedSinusoidFit(const float* y, int size, double initialOmega,
                       bool trend) {
  Fit best;
  double center = initialOmega;
  double radius = 2.0 * kPi / size;
  // DFT bins are only a starting point. Frequency is nonlinear, so refine it
  // locally while solving amplitude, phase and trend exactly at each trial.
  for (int pass = 0; pass < 3; ++pass) {
    const double passCenter = center;
    for (int step = -20; step <= 20; ++step) {
      const double omega = passCenter + radius * step / 20.0;
      if (omega <= 0.0 || omega > kPi) continue;
      Fit candidate = sinusoidFit(y, size, omega, trend);
      if (candidate.rmse < best.rmse) {
        best = std::move(candidate);
        center = omega;
      }
    }
    radius /= 10.0;
  }
  return best;
}

Fit twoSinusoidFit(const float* y, int size, double w1, double w2) {
  Fit fit = fitLinearBasis(y, size, 5,
      [w1, w2](double x, std::vector<double>& phi) {
        phi[0] = 1.0;
        phi[1] = std::sin(w1 * x); phi[2] = std::cos(w1 * x);
        phi[3] = std::sin(w2 * x); phi[4] = std::cos(w2 * x);
      });
  if (!std::isfinite(fit.maxAbsError)) return fit;
  const double amplitude1 = std::hypot(fit.coefficients[1], fit.coefficients[2]);
  const double phase1 = std::atan2(fit.coefficients[2], fit.coefficients[1]);
  const double amplitude2 = std::hypot(fit.coefficients[3], fit.coefficients[4]);
  const double phase2 = std::atan2(fit.coefficients[4], fit.coefficients[3]);
  fit.expression = number(fit.coefficients[0]) +
      "+(" + number(amplitude1) + "*sin((" + number(w1) + "*x)+" +
      number(phase1) + "))" +
      "+(" + number(amplitude2) + "*sin((" + number(w2) + "*x)+" +
      number(phase2) + "))";
  return fit;
}

Fit exponentialFit(const float* y, int size) {
  Fit best;
  if (size < 4) return best;
  // For each bounded growth rate, amplitude and offset are linear parameters.
  for (int step = -24; step <= 24; ++step) {
    if (step == 0) continue;
    const double rate = step * 4.0 / (24.0 * std::max(1, size - 1));
    Fit fit = fitLinearBasis(y, size, 2,
        [rate](double x, std::vector<double>& phi) {
          phi[0] = 1.0; phi[1] = std::exp(rate * x);
        });
    if (fit.maxAbsError < best.maxAbsError) {
      best = fit;
      best.expression = number(fit.coefficients[0]) + "+(" +
          number(fit.coefficients[1]) + "*exp(" + number(rate) + "*x))";
    }
  }
  return best;
}

Fit piecewiseLinearFit(const float* y, int size) {
  Fit best;
  if (size < 8) return best;
  for (int split = size / 5; split <= 4 * size / 5; split += std::max(1, size / 20)) {
    Fit left = polynomialFit(y, split, 1);
    Fit rightLocal = polynomialFit(y + split, size - split, 1);
    if (!std::isfinite(left.maxAbsError) || !std::isfinite(rightLocal.maxAbsError))
      continue;
    // Convert the right fit's local x to the block-local x used by the AST.
    const double rightSlope = rightLocal.coefficients[1];
    const double rightIntercept = rightLocal.coefficients[0] - rightSlope * split;
    double maxError = 0.0, squared = 0.0;
    for (int i = 0; i < size; ++i) {
      const double predicted = i < split
          ? left.coefficients[0] + left.coefficients[1] * i
          : rightIntercept + rightSlope * i;
      const double error = predicted - y[i];
      maxError = std::max(maxError, std::fabs(error));
      squared += error * error;
    }
    if (maxError < best.maxAbsError) {
      best.maxAbsError = maxError;
      best.rmse = std::sqrt(squared / size);
      // The AST has no conditional node. A steep logistic gives evolution a
      // usable, continuous approximation while preserving general fallback.
      const double steepness = 12.0 / std::max(1, size);
      const std::string leftExpr = "(" + number(left.coefficients[0]) + "+" +
          number(left.coefficients[1]) + "*x)";
      const std::string rightExpr = "(" + number(rightIntercept) + "+" +
          number(rightSlope) + "*x)";
      best.expression = leftExpr + "+(" + rightExpr + "-" + leftExpr +
          ")/(1+exp(" + number(-steepness) + "*(x-" + number(split) + ")))";
    }
  }
  return best;
}

double familyScore(const Fit& fit, double scale, double tolerance,
                   double complexityPenalty) {
  if (!std::isfinite(fit.maxAbsError)) return 0.0;
  const double normalized = fit.maxAbsError / std::max(scale, 1.0e-12);
  double score = 0.70 * std::exp(-3.0 * normalized) - complexityPenalty;
  if (fit.maxAbsError <= tolerance) score += 0.25;
  return std::max(0.0, std::min(0.99, score));
}

} // namespace

const char* curveFamilyName(CurveFamily family) {
  switch (family) {
    case CurveFamily::Constant: return "constant";
    case CurveFamily::Linear: return "linear";
    case CurveFamily::Quadratic: return "quadratic";
    case CurveFamily::Cubic: return "cubic";
    case CurveFamily::Sinusoid: return "sinusoid";
    case CurveFamily::SinusoidTrend: return "sinusoid+trend";
    case CurveFamily::TwoSinusoids: return "two-sinusoid";
    case CurveFamily::Exponential: return "exponential";
    case CurveFamily::PiecewiseLinear: return "piecewise-linear";
    case CurveFamily::GeneralSymbolic: return "general-symbolic";
  }
  return "unknown";
}

BlockCharacterization characterizeBlock(const float* data, long blockOffset,
                                         int blockSize, double tolerance) {
  BlockCharacterization result;
  result.tolerance = tolerance;
  if (!data || blockSize <= 0) return result;
  const float* y = data + blockOffset;
  double mean = 0.0, minValue = y[0], maxValue = y[0];
  for (int i = 0; i < blockSize; ++i) {
    mean += y[i]; minValue = std::min<double>(minValue, y[i]);
    maxValue = std::max<double>(maxValue, y[i]);
  }
  mean /= blockSize;
  double variance = 0.0;
  for (int i = 0; i < blockSize; ++i) variance += (y[i] - mean) * (y[i] - mean);
  result.dataRange = maxValue - minValue;
  result.standardDeviation = std::sqrt(variance / blockSize);
  const double scale = std::max({result.dataRange, result.standardDeviation,
                                 std::fabs(tolerance), 1.0e-12});

  std::vector<std::pair<CurveFamily, Fit>> fits;
  fits.emplace_back(CurveFamily::Constant, polynomialFit(y, blockSize, 0));
  fits.emplace_back(CurveFamily::Linear, polynomialFit(y, blockSize, 1));
  fits.emplace_back(CurveFamily::Quadratic, polynomialFit(y, blockSize, 2));
  fits.emplace_back(CurveFamily::Cubic, polynomialFit(y, blockSize, 3));
  const auto peaks = spectralPeaks(y, blockSize, 2);
  if (!peaks.empty()) {
    fits.emplace_back(CurveFamily::Sinusoid,
                      refinedSinusoidFit(y, blockSize, peaks[0].second, false));
    fits.emplace_back(CurveFamily::SinusoidTrend,
                      refinedSinusoidFit(y, blockSize, peaks[0].second, true));
  }
  if (peaks.size() >= 2)
    fits.emplace_back(CurveFamily::TwoSinusoids,
                      twoSinusoidFit(y, blockSize, peaks[0].second, peaks[1].second));
  fits.emplace_back(CurveFamily::Exponential, exponentialFit(y, blockSize));
  fits.emplace_back(CurveFamily::PiecewiseLinear, piecewiseLinearFit(y, blockSize));

  const std::array<double, 9> penalties =
      {0.00, 0.015, 0.035, 0.055, 0.045, 0.065, 0.09, 0.055, 0.08};
  for (std::size_t i = 0; i < fits.size(); ++i) {
    const Fit& fit = fits[i].second;
    result.rankedFamilies.push_back({fits[i].first,
        familyScore(fit, scale, tolerance, penalties[i]),
        fit.maxAbsError, fit.expression});
  }
  // Always keep the existing unrestricted evolutionary route represented.
  result.rankedFamilies.push_back(
      {CurveFamily::GeneralSymbolic, 0.10, std::numeric_limits<double>::infinity(), ""});
  std::stable_sort(result.rankedFamilies.begin(), result.rankedFamilies.end(),
      [](const FamilyScore& left, const FamilyScore& right) {
        return left.score > right.score;
      });
  return result;
}

std::vector<std::string> makeCharacterizedSeeds(
    const BlockCharacterization& characterization, int maxFamilies) {
  std::vector<std::string> result;
  if (maxFamilies <= 0) return result;
  double bestScore = 0.0;
  for (const FamilyScore& candidate : characterization.rankedFamilies) {
    if (!candidate.seedExpression.empty()) {
      bestScore = candidate.score;
      break;
    }
  }

  for (const FamilyScore& candidate : characterization.rankedFamilies) {
    if (static_cast<int>(result.size()) >= maxFamilies) break;
    if (candidate.seedExpression.empty()) continue;

    const bool withinTolerance =
        std::isfinite(candidate.maxAbsError) &&
        candidate.maxAbsError <= characterization.tolerance;
    const bool competitive =
        candidate.score >= 0.50 && candidate.score >= bestScore - 0.15;
    if (withinTolerance || competitive)
      result.push_back(candidate.seedExpression);
  }

  if (result.empty()) {
    for (const FamilyScore& candidate : characterization.rankedFamilies) {
      if (!candidate.seedExpression.empty()) {
        result.push_back(candidate.seedExpression);
        break;
      }
    }
  }
  return result;
}

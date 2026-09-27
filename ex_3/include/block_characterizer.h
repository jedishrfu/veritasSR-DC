#pragma once

#include <string>
#include <vector>

enum class CurveFamily {
  Constant,
  Linear,
  Quadratic,
  Cubic,
  Sinusoid,
  SinusoidTrend,
  TwoSinusoids,
  Exponential,
  PiecewiseLinear,
  GeneralSymbolic
};

struct FamilyScore {
  CurveFamily family;
  double score;
  double maxAbsError;
  std::string seedExpression;
};

struct BlockCharacterization {
  double dataRange = 0.0;
  double standardDeviation = 0.0;
  double tolerance = 0.0;
  std::vector<FamilyScore> rankedFamilies;
};

// x is the zero-based sample index within the block, matching NodeStats.
BlockCharacterization characterizeBlock(
    const float* data,
    long blockOffset,
    int blockSize,
    double tolerance);

// Returns only fitted AST seeds supported by the characterization. If no
// family clears the confidence gate, the best fitted family is returned.
std::vector<std::string> makeCharacterizedSeeds(
    const BlockCharacterization& characterization,
    int maxFamilies = 3);

const char* curveFamilyName(CurveFamily family);

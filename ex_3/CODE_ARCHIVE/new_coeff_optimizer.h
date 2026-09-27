#pragma once

class Node;

struct CoeffOptResult {
  int numCoeffs = 0;
  int passes = 0;
  int evals = 0;
  double finalStep = 0.0;

  double mae = 0.0;
  double mse = 0.0;
  double rmse = 0.0;
  double maxAbsError = 0.0;
};

CoeffOptResult optimizeNodeCoeffsCoordinateDescent(
  Node* root,
  const float* data,
  int numFloats,
  double initialStep = 1.0,
  double minStep = 1e-6,
  int maxPasses = 50,
  int maxMovesPerCoeff = 64,
  double stepShrink = 0.5,
  double rmseGuard = 1.05,
  bool doGroupRefinement = true,
  bool verboseLogging = true
);
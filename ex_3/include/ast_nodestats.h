#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <string>
#include <vector>
#include <set>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <cmath>
#include <string>

class NodeStats
{
public:
  long count;

  long numWithinTol;
  long numOutsideTol;

  double sumError;
  double sumSquaredError;
  double maxAbsError;
  double meanOriginal;

  double mae;
  double mse;
  double rmse;
  double psnr;

  int nodeCount;
  int depth;

  NodeStats()
    : count(0),
    numWithinTol(0),
    numOutsideTol(0),
    sumError(0.0),
    sumSquaredError(0.0),
    maxAbsError(0.0),
    meanOriginal(0.0),
    mae(0.0),
    mse(0.0),
    rmse(0.0),
    psnr(0.0),
    nodeCount(0),
    depth(0) {
  }

  void computeScore(int vi, double tol, float* data, int numFloats, Node* n, NodeStats* ns);

  std::string toString() const
  {
    std::ostringstream ss;

    ss << "count=" << count
      << " nodes=" << nodeCount
      << " depth=" << depth
      << " mae=" << mae
      << " mse=" << mse
      << " rmse=" << rmse
      << " psnr=" << psnr
      << " maxAbs=" << maxAbsError
      << " within=" << numWithinTol
      << " outside=" << numOutsideTol;

    return ss.str();
  }
};


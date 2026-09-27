#pragma once

#include <string>
#include <sstream>
#include <iostream>

#include "../include/ast_nodes.h"

class NodeStats {
public:
  long blockSize;

  long numWithinTol;
  long numOutsideTol;
  long numNaN;
  long numInfinity;

  double sumError;
  double sumSquaredError;
  double maxAbsError;
  double meanDataInput;

  double mae;
  double mse;
  double rmse;
  double psnr;

  int nodeCount;
  int depth;

  NodeStats()
    : blockSize(0),
      numWithinTol(0),
      numOutsideTol(0),
      numNaN(0),
      numInfinity(0),
      sumError(0.0),
      sumSquaredError(0.0),
      maxAbsError(0.0),
      meanDataInput(0.0),
      mae(0.0),
      mse(0.0),
      rmse(0.0),
      psnr(0.0),
      nodeCount(0),
      depth(0) {}

  static void computeScore(int blockID, int genID, int stepID, int varID, const Options& opts,
    float* data,  Node* n, NodeStats* ns);

  std::string toString() const {
    std::ostringstream ss;

    ss << "blockSize=" << blockSize
      << " nodes=" << nodeCount
      << " depth=" << depth
      << " mae=" << mae
      << " mse=" << mse
      << " rmse=" << rmse
      << " psnr=" << psnr
      << " maxAbs=" << maxAbsError
      << " within=" << numWithinTol
      << " outside=" << numOutsideTol
      << " nan=" << numNaN
      << " infinity=" << numInfinity;

    return ss.str();
  }
};

NodeStats averageNodeStats(const ExprArray& pool);

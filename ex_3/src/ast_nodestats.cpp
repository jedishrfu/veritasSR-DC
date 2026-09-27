#include <string>
#include <set>
#include <cmath>
#include <unistd.h>

#include "ast_nodes.h"
#include "ast_nodestats.h"
#include "expr_array.h"
#include "var_table.h"

extern thread_local VarTable varTable;

void NodeStats::computeScore(
  int blockID,
  int genID,
  int stepID,
  int varID,
  const Options& opts,
  float* data,
  Node* n,
  NodeStats* ns) {
  if (ns == nullptr)
    return;

  (void) blockID;
  (void) genID;
  (void) stepID;

  *ns = NodeStats();

  ns->blockSize = opts.blockSize;
  ns->nodeCount = countNodes(n);
  ns->depth = treeDepth(n);


  double peakValue = 0.0;

  const long blockOffset = opts.blockOffset;
  //logPrint("### blockID=%d, blockSize=%d, i=%d",blockID,opts.blockSize,blockOffset);

  for (int i = 0; i < opts.blockSize; i++) {

    varTable.setValue(varID, static_cast<double>(i));
    double computed = n->eval();

    if (std::isnan(computed))
      ns->numNaN++;
    else if (std::isinf(computed))
      ns->numInfinity++;

    // if (computed != computed)
    // {
    //   computed = 1.0e99;
    // }

    double error = computed - data[i+blockOffset];
    double absError = fabs(error);

    ns->sumError += absError;
    ns->sumSquaredError += error * error;
    ns->meanDataInput += data[i+blockOffset];

    if (absError > ns->maxAbsError) {
      ns->maxAbsError = absError;
    }

    if (absError <= opts.tol) {
      ns->numWithinTol++;
    }
    else {
      ns->numOutsideTol++;
    }

    if (fabs(data[i+blockOffset]) > peakValue) {
      peakValue = fabs(data[i+blockOffset]);
    }
  }

  if (opts.blockSize > 0) {
    ns->meanDataInput /= opts.blockSize;
    ns->mae = ns->sumError / opts.blockSize;
    ns->mse = ns->sumSquaredError / opts.blockSize;
    ns->rmse = sqrt(ns->mse);

    if (ns->rmse > 0.0 && peakValue > 0.0) {
      ns->psnr = 20.0 * log10(peakValue / ns->rmse);
    }
    else {
      ns->psnr = 999.0;
    }
  }
}

NodeStats averageNodeStats(const ExprArray& pool) {
  NodeStats avg;

  if (pool.items.empty())
    return avg;

  int numValidExprs = 0;

  for (const auto es : pool.items) {
    if (es == nullptr || es->ns == nullptr) continue;

    const NodeStats& s = *es->ns;

    if (numValidExprs==0) avg.blockSize = s.blockSize;  // blockSize never changes

    avg.numWithinTol += s.numWithinTol;
    avg.numOutsideTol += s.numOutsideTol;
    avg.numNaN += s.numNaN;
    avg.numInfinity += s.numInfinity;

    avg.sumError += s.sumError;
    avg.sumSquaredError += s.sumSquaredError;
    avg.maxAbsError += s.maxAbsError;
    avg.meanDataInput += s.meanDataInput;

    if (!std::isfinite(s.mae) || !std::isfinite(s.mse) || !std::isfinite(s.rmse)) continue;

    avg.mae += s.mae;
    avg.mse += s.mse;
    avg.rmse += s.rmse;
    avg.psnr += s.psnr;

    avg.nodeCount += s.nodeCount;
    avg.depth += s.depth;

    numValidExprs++;
  }

  if (numValidExprs == 0)
    return avg;

  avg.numWithinTol /= numValidExprs;
  avg.numOutsideTol /= numValidExprs;
  avg.numNaN /= numValidExprs;
  avg.numInfinity /= numValidExprs;

  avg.sumError /= numValidExprs;
  avg.sumSquaredError /= numValidExprs;
  avg.maxAbsError /= numValidExprs;
  avg.meanDataInput /= numValidExprs;

  avg.mae /= numValidExprs;
  avg.mse /= numValidExprs;
  avg.rmse /= numValidExprs;
  avg.psnr /= numValidExprs;

  avg.nodeCount /= numValidExprs;
  avg.depth /= numValidExprs;

  return avg;
}

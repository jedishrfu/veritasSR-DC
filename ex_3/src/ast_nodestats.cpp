#include <string>
#include <set>
#include <unistd.h>

#include "../include/ast_nodes.h"
#include "../include/ast_nodestats.h"
#include "../include/expr_array.h"

void NodeStats::computeScore(
  int vi,
  double tol,
  float* data,
  int numFloats,
  Node* n,
  NodeStats* ns)
{
  if (ns == nullptr)
    return;

  *ns = NodeStats();

  ns->count = numFloats;
  ns->nodeCount = countNodes(n);
  ns->depth = treeDepth(n);

  double peakValue = 0.0;

  for (int i = 0; i < numFloats; i++)
  {
    varTable.setValue(vi, static_cast<double>(i));
    double computed = n->eval();

    if (computed != computed)
    {
      computed = 1.0e99;
    }

    double error = computed - data[i];
    double absError = fabs(error);

    ns->sumError += absError;
    ns->sumSquaredError += error * error;
    ns->meanOriginal += data[i];

    if (absError > ns->maxAbsError)
    {
      ns->maxAbsError = absError;
    }

    if (absError <= tol)
    {
      ns->numWithinTol++;
    }
    else
    {
      ns->numOutsideTol++;
    }

    if (fabs(data[i]) > peakValue)
    {
      peakValue = fabs(data[i]);
    }
  }

  if (numFloats > 0)
  {
    ns->meanOriginal /= numFloats;
    ns->mae = ns->sumError / numFloats;
    ns->mse = ns->sumSquaredError / numFloats;
    ns->rmse = sqrt(ns->mse);

    if (ns->rmse > 0.0 && peakValue > 0.0)
    {
      ns->psnr = 20.0 * log10(peakValue / ns->rmse);
    }
    else
    {
      ns->psnr = 999.0;
    }
  }
}

inline NodeStats averageNodeStats(const ExprArray& pool)
{
  NodeStats avg;

  if (pool.items.empty())
    return avg;

  int validCount = 0;

  for (const auto es: pool.items)
  {
    if (es == nullptr || es->ns == nullptr) continue;

    const NodeStats& s = *es->ns;

    avg.count += s.count;
    avg.numWithinTol += s.numWithinTol;
    avg.numOutsideTol += s.numOutsideTol;

    avg.sumError += s.sumError;
    avg.sumSquaredError += s.sumSquaredError;
    avg.maxAbsError += s.maxAbsError;
    avg.meanOriginal += s.meanOriginal;

    if (!std::isfinite(s.mae) || !std::isfinite(s.mse) || !std::isfinite(s.rmse)) continue;

    avg.mae += s.mae;
    avg.mse += s.mse;
    avg.rmse += s.rmse;
    avg.psnr += s.psnr;

    avg.nodeCount += s.nodeCount;
    avg.depth += s.depth;

    validCount++;
  }

  if (validCount == 0)
    return avg;

  avg.count /= validCount;
  avg.numWithinTol /= validCount;
  avg.numOutsideTol /= validCount;

  avg.sumError /= validCount;
  avg.sumSquaredError /= validCount;
  avg.maxAbsError /= validCount;
  avg.meanOriginal /= validCount;

  avg.mae /= validCount;
  avg.mse /= validCount;
  avg.rmse /= validCount;
  avg.psnr /= validCount;

  avg.nodeCount /= validCount;
  avg.depth /= validCount;

  return avg;
}
#include <cmath>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>

#include "ast_nodes.h"
#include "ast_nodestats.h"

inline void coeffTrace(const std::string& msg) {
  logPrint(("- " + msg).c_str());;
}

inline std::string coeffListString(
  int lo,
  int hi,
  const std::vector<Node*>& coeffs) {

  std::ostringstream oss;
  oss << "{";

  for (int i = lo; i < hi; i++) {
    if (i > lo)
      oss << ", ";

    oss << i << ":" << std::setprecision(12)
        << coeffs[i]->getNodeCoeff();
  }

  oss << "}";
  return oss.str();
}

inline std::string stepListString(
  int lo,
  const std::vector<double>& steps) {

  std::ostringstream oss;
  oss << "{";

  for (int i = 0; i < static_cast<int>(steps.size()); i++) {
    if (i > 0)
      oss << ", ";

    oss << lo + i << ":" << std::setprecision(12) << steps[i];
  }

  oss << "}";
  return oss.str();
}

inline void traceSubset(
  const char* label,
  int blockID,
  int genID,
  int stepID,
  int varID,
  int lo,
  int hi,
  const std::vector<Node*>& coeffs,
  double maxAbsErr,
  int traceLevel) {

  if (traceLevel < 2)
    return;

  std::ostringstream oss;

  oss << "[COEFF_TRACE] "
      << label
      << " block=" << blockID
      << " gen=" << genID
      << " step=" << stepID
      << " var=" << varID
      << " range=[" << lo << "," << hi << ")"
      << " maxAbsErr=" << std::setprecision(12) << maxAbsErr
      << " coeffs=" << coeffListString(lo, hi, coeffs);

  coeffTrace(oss.str());
}

inline bool isIntrinsicOp(OpKind op) {
  return op == OP_SIN || op == OP_COS || op == OP_EXP || op == OP_LOG;
}

// Keep coefficients with similar numerical influence in the same search group.
// Group 0 contains coefficients outside intrinsic functions.  Each intrinsic
// introduces a new group for coefficients directly in its argument; a nested
// intrinsic introduces another group of its own.
inline void collectCoeffGroups(
  Node* node,
  std::vector<std::vector<Node*>>& groups,
  size_t groupIndex) {

  if (node == nullptr)
    return;

  if (node->isCoeffNode()) {
    groups[groupIndex].push_back(node);
    return;
  }

  if (node->getKind() == NODE_UNARY && isIntrinsicOp(node->getOp())) {
    groups.emplace_back();
    const size_t intrinsicGroupIndex = groups.size() - 1;
    collectCoeffGroups(node->getLeftChild(), groups, intrinsicGroupIndex);
    return;
  }

  collectCoeffGroups(node->getLeftChild(), groups, groupIndex);
  collectCoeffGroups(node->getRightChild(), groups, groupIndex);
}

inline std::vector<std::vector<Node*>> extractCoeffGroups(Node* root) {
  std::vector<std::vector<Node*>> groups(1);
  collectCoeffGroups(root, groups, 0);

  groups.erase(
    std::remove_if(
      groups.begin(),
      groups.end(),
      [](const std::vector<Node*>& group) { return group.empty(); }),
    groups.end());

  return groups;
}

inline double scoreMaxAbsErr(
  int blockID,
  int genID,
  int stepID,
  int varID,
  const Options& opts,
  Node* n,
  float* data) {

  NodeStats ns;

  NodeStats::computeScore(
    blockID,
    genID,
    stepID,
    varID,
    opts,
    data,
    n,
    &ns);

  return ns.maxAbsError;
}

inline double optimizeCoeffSubset_HillClimbing_Search(
  int blockID,
  int genID,
  int stepID,
  int varID,
  const Options& opts,
  Node* n,
  const std::vector<Node*>& coeffs,
  int lo,
  int hi,
  float* data,
  double initialStep,
  int maxIterations,
  int traceLevel) {

  const int blockSize = opts.blockSize;
  const double tolerance = std::max(opts.tol, 1.0e-12);

  if (blockSize <= 0)
    return 1.0e99;

  int count = hi - lo;

  if (count <= 0)
    return scoreMaxAbsErr(blockID, genID, stepID, varID, opts, n, data);

  double bestMaxAbsErr =
    scoreMaxAbsErr(blockID, genID, stepID, varID, opts, n, data);

  traceSubset(
    "ENTER_SUBSET",
    blockID,
    genID,
    stepID,
    varID,
    lo,
    hi,
    coeffs,
    bestMaxAbsErr,
    traceLevel);

  std::vector<double> step(count, initialStep);

  for (int i = 0; i < count; i++) {
    Node* c = coeffs[lo + i];
    double original = c->getNodeCoeff();

    c->setNodeCoeff(original + initialStep);
    double plusErr =
      scoreMaxAbsErr(blockID, genID, stepID, varID, opts, n, data);

    c->setNodeCoeff(original - initialStep);
    double minusErr =
      scoreMaxAbsErr(blockID, genID, stepID, varID, opts, n, data);

    c->setNodeCoeff(original);

    if (plusErr < bestMaxAbsErr && plusErr <= minusErr)
      step[i] = fabs(initialStep);
    else if (minusErr < bestMaxAbsErr)
      step[i] = -fabs(initialStep);
    else
      // Neither full-size probe improved the score. Keep searching: the
      // rejection path shrinks and reverses the step to find smaller moves.
      step[i] = fabs(initialStep);

    std::ostringstream oss;

    oss << "[COEFF_TRACE] INIT_SIGN"
        << " block=" << blockID
        << " gen=" << genID
        << " step=" << stepID
        << " var=" << varID
        << " coeffIndex=" << lo + i
        << " original=" << std::setprecision(12) << original
        << " plusErr=" << plusErr
        << " minusErr=" << minusErr
        << " bestErr=" << bestMaxAbsErr
        << " chosenStep=" << step[i];

    if (traceLevel >= 2)
      coeffTrace(oss.str());
  }

  for (int iter = 0; iter < maxIterations; iter++) {
    double maxStep = 0.0;

    for (int i = 0; i < count; i++)
      maxStep = std::max(maxStep, fabs(step[i]));

    if (maxStep < tolerance) {
      std::ostringstream oss;

      oss << "[COEFF_TRACE] STOP_TOLERANCE"
          << " block=" << blockID
          << " gen=" << genID
          << " step=" << stepID
          << " var=" << varID
          << " iter=" << iter
          << " range=[" << lo << "," << hi << ")"
          << " maxStep=" << std::setprecision(12) << maxStep
          << " tolerance=" << tolerance
          << " bestMaxAbsErr=" << bestMaxAbsErr
          << " coeffs=" << coeffListString(lo, hi, coeffs);

      if (traceLevel >= 2)
        coeffTrace(oss.str());
      break;
    }

    std::vector<double> oldValues(count);

    for (int i = 0; i < count; i++) {
      Node* c = coeffs[lo + i];

      oldValues[i] = c->getNodeCoeff();
      c->setNodeCoeff(oldValues[i] + step[i]);
    }

    double candidateErr =
      scoreMaxAbsErr(blockID, genID, stepID, varID, opts, n, data);

    bool accepted = candidateErr < bestMaxAbsErr;

    std::vector<double> candidateValues(count);

    for (int i = 0; i < count; i++)
      candidateValues[i] = coeffs[lo + i]->getNodeCoeff();

    std::vector<double> nextStep = step;

    if (accepted) {
      bestMaxAbsErr = candidateErr;

      for (int i = 0; i < count; i++)
        nextStep[i] = step[i] * 2.0;
    }
    else {
      for (int i = 0; i < count; i++)
        coeffs[lo + i]->setNodeCoeff(oldValues[i]);

      for (int i = 0; i < count; i++)
        nextStep[i] = step[i] * -0.5;
    }

    std::ostringstream oldCoeffOss;
    oldCoeffOss << "{";

    for (int i = 0; i < count; i++) {
      if (i > 0)
        oldCoeffOss << ", ";

      oldCoeffOss << lo + i << ":" << std::setprecision(12) << oldValues[i];
    }

    oldCoeffOss << "}";

    std::ostringstream candidateCoeffOss;
    candidateCoeffOss << "{";

    for (int i = 0; i < count; i++) {
      if (i > 0)
        candidateCoeffOss << ", ";

      candidateCoeffOss << lo + i << ":" << std::setprecision(12)
                        << candidateValues[i];
    }

    candidateCoeffOss << "}";

    std::ostringstream oss;

    oss << "[COEFF_TRACE] TRY"
        << " block=" << blockID
        << " gen=" << genID
        << " step=" << stepID
        << " var=" << varID
        << " iter=" << iter
        << " range=[" << lo << "," << hi << ")"
        << " oldCoeffs=" << oldCoeffOss.str()
        << " trySteps=" << stepListString(lo, step)
        << " candidateCoeffs=" << candidateCoeffOss.str()
        << " candidateErr=" << std::setprecision(12) << candidateErr
        << " bestErr=" << bestMaxAbsErr
        << " result=" << (accepted ? "ACCEPT" : "REJECT")
        << " nextSteps=" << stepListString(lo, nextStep)
        << " finalCoeffs=" << coeffListString(lo, hi, coeffs);

    if (traceLevel >= 2)
      coeffTrace(oss.str());

    step = nextStep;
  }

  if (count > 1) {
    int mid = lo + count / 2;

    double beforeLeft =
      scoreMaxAbsErr(blockID, genID, stepID, varID, opts, n, data);

    traceSubset(
      "RECURSE_LEFT_BEGIN",
      blockID,
      genID,
      stepID,
      varID,
      lo,
      mid,
      coeffs,
      beforeLeft,
      traceLevel);

    double leftErr = optimizeCoeffSubset_HillClimbing_Search(
      blockID,
      genID,
      stepID,
      varID,
      opts,
      n,
      coeffs,
      lo,
      mid,
      data,
      initialStep,
      maxIterations,
      traceLevel);

    double beforeRight =
      scoreMaxAbsErr(blockID, genID, stepID, varID, opts, n, data);

    traceSubset(
      "RECURSE_RIGHT_BEGIN",
      blockID,
      genID,
      stepID,
      varID,
      mid,
      hi,
      coeffs,
      beforeRight,
      traceLevel);

    double rightErr = optimizeCoeffSubset_HillClimbing_Search(
      blockID,
      genID,
      stepID,
      varID,
      opts,
      n,
      coeffs,
      mid,
      hi,
      data,
      initialStep,
      maxIterations,
      traceLevel);

    bestMaxAbsErr = std::min(bestMaxAbsErr, leftErr);
    bestMaxAbsErr = std::min(bestMaxAbsErr, rightErr);
    bestMaxAbsErr = std::min(bestMaxAbsErr, beforeLeft);
    bestMaxAbsErr = std::min(bestMaxAbsErr, beforeRight);
  }

  double finalErr =
    scoreMaxAbsErr(blockID, genID, stepID, varID, opts, n, data);

  bestMaxAbsErr = std::min(bestMaxAbsErr, finalErr);

  traceSubset(
    "EXIT_SUBSET",
    blockID,
    genID,
    stepID,
    varID,
    lo,
    hi,
    coeffs,
    bestMaxAbsErr,
    traceLevel);

  return bestMaxAbsErr;
}

double optimize_NodeCoeffs_HillClimbing_Search(
  int blockID,
  int genID,
  int stepID,
  int varID,
  const Options& opts,
  Node* n,
  float* data,
  double initialStep = 1.0,
  int maxIterations = 1000,
  int traceLevel = 0) {

  if (n == nullptr || data == nullptr || opts.blockSize <= 0)
    return 1.0e99;

  std::vector<std::vector<Node*>> coeffGroups = extractCoeffGroups(n);

  if (coeffGroups.empty())
    return scoreMaxAbsErr(blockID, genID, stepID, varID, opts, n, data);

  double initialErr =
    scoreMaxAbsErr(blockID, genID, stepID, varID, opts, n, data);

  traceLevel = std::max(0, std::min(2, traceLevel));

  if (traceLevel >= 1)
    logPrint("\n### Expression: %s", n->canonicalString().c_str());

  double finalErr = initialErr;

  for (size_t groupIndex = 0; groupIndex < coeffGroups.size(); groupIndex++) {
    std::vector<Node*>& coeffs = coeffGroups[groupIndex];

    if (traceLevel >= 1) {
      std::ostringstream oss;
      oss << "[COEFF_TRACE] OPTIMIZE_GROUP"
          << " group=" << groupIndex
          << " coefficientCount=" << coeffs.size();
      coeffTrace(oss.str());
    }

    traceSubset(
      "OPTIMIZE_BEGIN",
      blockID,
      genID,
      stepID,
      varID,
      0,
      static_cast<int>(coeffs.size()),
      coeffs,
      finalErr,
      traceLevel == 1 ? 2 : traceLevel);

    finalErr = optimizeCoeffSubset_HillClimbing_Search(
      blockID,
      genID,
      stepID,
      varID,
      opts,
      n,
      coeffs,
      0,
      static_cast<int>(coeffs.size()),
      data,
      initialStep,
      maxIterations,
      traceLevel);

    traceSubset(
      "OPTIMIZE_END",
      blockID,
      genID,
      stepID,
      varID,
      0,
      static_cast<int>(coeffs.size()),
      coeffs,
      finalErr,
      traceLevel == 1 ? 2 : traceLevel);
  }

  // Report the score of the final combined state rather than the best score
  // returned by an intermediate group.
  finalErr = scoreMaxAbsErr(blockID, genID, stepID, varID, opts, n, data);

  if (traceLevel >= 1)
    logPrint("\n-- Expression: %s", n->canonicalString().c_str());

  return finalErr;
}

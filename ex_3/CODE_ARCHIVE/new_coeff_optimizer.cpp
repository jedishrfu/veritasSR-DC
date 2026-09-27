#include "coeff_optimizer.h"

#include <cmath>
#include <limits>
#include <algorithm>

#include "ast_nodes.h"
#include "var_table.h"
#include "util_code.h"

extern VarTable varTable;

struct LocalScore {
  double mae = std::numeric_limits<double>::infinity();
  double mse = std::numeric_limits<double>::infinity();
  double rmse = std::numeric_limits<double>::infinity();
  double maxAbsError = std::numeric_limits<double>::infinity();
};

static void logScore(const char* title, const LocalScore& s) {
  logPrint(
    "%s\n"
    "  MAE         = %.12g\n"
    "  MSE         = %.12g\n"
    "  RMSE        = %.12g\n"
    "  MaxAbsError = %.12g\n",
    title,
    s.mae,
    s.mse,
    s.rmse,
    s.maxAbsError
  );
}

static void logCoefficients(
  Node* root,
  int numCoeffs,
  const char* title) {

  logPrint("\n%s\n", title);

  if (!root) {
    logPrint("  <null root>\n");
    return;
  }

  for (int i = 0; i < numCoeffs; i++) {
    logPrint("  c[%2d] = %.12f\n", i, root->getNodeCoeff(i));
  }
}

static bool isBetter(
  const LocalScore& candidate,
  const LocalScore& current,
  double rmseGuard) {

  if (!std::isfinite(candidate.maxAbsError) ||
      !std::isfinite(candidate.rmse)) {
    return false;
  }

  if (candidate.maxAbsError < current.maxAbsError) {
    if (!std::isfinite(current.rmse))
      return true;

    return candidate.rmse <= current.rmse * rmseGuard;
  }

  return false;
}

static LocalScore scoreNode(
  Node* root,
  const float* data,
  int numFloats,
  int& evalCounter) {

  LocalScore s;

  if (!root || !data || numFloats <= 0)
    return s;

  double sumAbs = 0.0;
  double sumSq = 0.0;
  double maxAbs = 0.0;

  for (int i = 0; i < numFloats; i++) {
    varTable.setValue(0, static_cast<double>(i));

    double yhat = root->eval();
    double y = static_cast<double>(data[i]);

    if (!std::isfinite(yhat))
      return s;

    double err = yhat - y;
    double absErr = std::abs(err);

    sumAbs += absErr;
    sumSq += err * err;
    maxAbs = std::max(maxAbs, absErr);
  }

  evalCounter++;

  s.mae = sumAbs / numFloats;
  s.mse = sumSq / numFloats;
  s.rmse = std::sqrt(s.mse);
  s.maxAbsError = maxAbs;

  return s;
}

static bool tryMoveCoeff(
  Node* root,
  int coeffIndex,
  double delta,
  const float* data,
  int numFloats,
  LocalScore& best,
  int& evalCounter,
  double rmseGuard,
  int numCoeffs,
  bool verboseLogging) {

  double oldValue = root->getNodeCoeff(coeffIndex);
  double newValue = oldValue + delta;

  if (verboseLogging) {
    logPrint(
      "\n--------------------------------------------------\n"
      "TRY COEFFICIENT MOVE\n"
      "  coeffIndex = %d\n"
      "  oldValue   = %.12f\n"
      "  delta      = %+ .12g\n"
      "  newValue   = %.12f\n",
      coeffIndex,
      oldValue,
      delta,
      newValue
    );

    logCoefficients(root, numCoeffs, "Before change:");
    logScore("Current best score:", best);
  }

  root->setNodeCoeff(coeffIndex, newValue);

  if (verboseLogging) {
    logCoefficients(root, numCoeffs, "After change:");
  }

  LocalScore candidate = scoreNode(root, data, numFloats, evalCounter);
  bool accepted = isBetter(candidate, best, rmseGuard);

  if (verboseLogging) {
    logScore("Candidate score:", candidate);

    logPrint(
      "Decision:\n"
      "  accepted = %s\n",
      accepted ? "YES" : "NO"
    );
  }

  if (accepted) {
    best = candidate;

    if (verboseLogging) {
      logPrint(
        "ACCEPTED\n"
        "  c[%d] %.12f -> %.12f\n",
        coeffIndex,
        oldValue,
        newValue
      );

      logCoefficients(root, numCoeffs, "After accepted move:");
    }

    return true;
  }

  root->setNodeCoeff(coeffIndex, oldValue);

  if (verboseLogging) {
    logPrint(
      "REJECTED -- restored\n"
      "  c[%d] %.12f -> %.12f\n",
      coeffIndex,
      newValue,
      oldValue
    );

    logCoefficients(root, numCoeffs, "After restore:");
  }

  return false;
}

static bool optimizeOneCoeff(
  Node* root,
  int coeffIndex,
  double step,
  const float* data,
  int numFloats,
  LocalScore& best,
  int& evalCounter,
  int maxMovesPerCoeff,
  double rmseGuard,
  int numCoeffs,
  bool verboseLogging) {

  if (verboseLogging) {
    logPrint(
      "\n==================================================\n"
      "OPTIMIZE SINGLE COEFFICIENT\n"
      "  coeffIndex = %d\n"
      "  step       = %.12g\n"
      "==================================================\n",
      coeffIndex,
      step
    );

    logCoefficients(root, numCoeffs, "Coefficient state entering single-coeff optimization:");
  }

  bool plusWorks = tryMoveCoeff(
    root,
    coeffIndex,
    step,
    data,
    numFloats,
    best,
    evalCounter,
    rmseGuard,
    numCoeffs,
    verboseLogging
  );

  bool minusWorks = false;

  if (!plusWorks) {
    minusWorks = tryMoveCoeff(
      root,
      coeffIndex,
      -step,
      data,
      numFloats,
      best,
      evalCounter,
      rmseGuard,
      numCoeffs,
      verboseLogging
    );
  }

  if (!plusWorks && !minusWorks) {
    if (verboseLogging) {
      logPrint(
        "No improvement for c[%d] at step %.12g\n",
        coeffIndex,
        step
      );
    }

    return false;
  }

  double direction = plusWorks ? step : -step;

  if (verboseLogging) {
    logPrint(
      "Direction selected for c[%d]: %+ .12g\n",
      coeffIndex,
      direction
    );
  }

  for (int move = 1; move < maxMovesPerCoeff; move++) {
    if (verboseLogging) {
      logPrint(
        "\nContinue moving c[%d]\n"
        "  move number = %d\n"
        "  direction   = %+ .12g\n",
        coeffIndex,
        move,
        direction
      );
    }

    bool moved = tryMoveCoeff(
      root,
      coeffIndex,
      direction,
      data,
      numFloats,
      best,
      evalCounter,
      rmseGuard,
      numCoeffs,
      verboseLogging
    );

    if (!moved) {
      if (verboseLogging) {
        logPrint(
          "Stopped moving c[%d]; next move failed.\n",
          coeffIndex
        );
      }

      break;
    }
  }

  if (verboseLogging) {
    logCoefficients(root, numCoeffs, "Coefficient state leaving single-coeff optimization:");
  }

  return true;
}

static bool tryMoveCoeffRange(
  Node* root,
  int beginCoeff,
  int endCoeff,
  double delta,
  const float* data,
  int numFloats,
  LocalScore& best,
  int& evalCounter,
  double rmseGuard,
  int numCoeffs,
  bool verboseLogging) {

  if (beginCoeff >= endCoeff)
    return false;

  if (verboseLogging) {
    logPrint(
      "\n--------------------------------------------------\n"
      "TRY GROUP MOVE\n"
      "  beginCoeff = %d\n"
      "  endCoeff   = %d\n"
      "  delta      = %+ .12g\n",
      beginCoeff,
      endCoeff,
      delta
    );

    logCoefficients(root, numCoeffs, "Before group change:");
    logScore("Current best score:", best);
  }

  for (int i = beginCoeff; i < endCoeff; i++) {
    double v = root->getNodeCoeff(i);
    root->setNodeCoeff(i, v + delta);
  }

  if (verboseLogging) {
    logCoefficients(root, numCoeffs, "After group change:");
  }

  LocalScore candidate = scoreNode(root, data, numFloats, evalCounter);
  bool accepted = isBetter(candidate, best, rmseGuard);

  if (verboseLogging) {
    logScore("Candidate group score:", candidate);

    logPrint(
      "Group decision:\n"
      "  accepted = %s\n",
      accepted ? "YES" : "NO"
    );
  }

  if (accepted) {
    best = candidate;

    if (verboseLogging) {
      logCoefficients(root, numCoeffs, "After accepted group move:");
    }

    return true;
  }

  for (int i = beginCoeff; i < endCoeff; i++) {
    double v = root->getNodeCoeff(i);
    root->setNodeCoeff(i, v - delta);
  }

  if (verboseLogging) {
    logPrint("GROUP MOVE REJECTED -- restored coefficients.\n");
    logCoefficients(root, numCoeffs, "After group restore:");
  }

  return false;
}

static bool optimizeCoeffGroupRecursive(
  Node* root,
  int beginCoeff,
  int endCoeff,
  double step,
  const float* data,
  int numFloats,
  LocalScore& best,
  int& evalCounter,
  double rmseGuard,
  int numCoeffs,
  bool verboseLogging) {

  int n = endCoeff - beginCoeff;

  if (n <= 1)
    return false;

  if (verboseLogging) {
    logPrint(
      "\n==================================================\n"
      "GROUP REFINEMENT\n"
      "  beginCoeff = %d\n"
      "  endCoeff   = %d\n"
      "  count      = %d\n"
      "  step       = %.12g\n"
      "==================================================\n",
      beginCoeff,
      endCoeff,
      n,
      step
    );
  }

  bool improved = false;

  improved |= tryMoveCoeffRange(
    root,
    beginCoeff,
    endCoeff,
    step,
    data,
    numFloats,
    best,
    evalCounter,
    rmseGuard,
    numCoeffs,
    verboseLogging
  );

  if (!improved) {
    improved |= tryMoveCoeffRange(
      root,
      beginCoeff,
      endCoeff,
      -step,
      data,
      numFloats,
      best,
      evalCounter,
      rmseGuard,
      numCoeffs,
      verboseLogging
    );
  }

  int mid = beginCoeff + n / 2;

  improved |= optimizeCoeffGroupRecursive(
    root,
    beginCoeff,
    mid,
    step,
    data,
    numFloats,
    best,
    evalCounter,
    rmseGuard,
    numCoeffs,
    verboseLogging
  );

  improved |= optimizeCoeffGroupRecursive(
    root,
    mid,
    endCoeff,
    step,
    data,
    numFloats,
    best,
    evalCounter,
    rmseGuard,
    numCoeffs,
    verboseLogging
  );

  return improved;
}

CoeffOptResult optimizeNodeCoeffsCoordinateDescent(
  Node* root,
  const float* data,
  int numFloats,
  double initialStep,
  double minStep,
  int maxPasses,
  int maxMovesPerCoeff,
  double stepShrink,
  double rmseGuard,
  bool doGroupRefinement,
  bool verboseLogging) {

  CoeffOptResult result;

  if (!root || !data || numFloats <= 0)
    return result;

  int evalCounter = 0;
  int numCoeffs = countNodeCoeffs(root);

  result.numCoeffs = numCoeffs;

  if (numCoeffs <= 0)
    return result;

  LocalScore best = scoreNode(root, data, numFloats, evalCounter);

  double step = initialStep;

  if (verboseLogging) {
    logPrint(
      "\n##################################################\n"
      "START COEFFICIENT OPTIMIZATION\n"
      "##################################################\n"
      "numCoeffs        = %d\n"
      "numFloats        = %d\n"
      "initialStep      = %.12g\n"
      "minStep          = %.12g\n"
      "maxPasses        = %d\n"
      "maxMovesPerCoeff = %d\n"
      "stepShrink       = %.12g\n"
      "rmseGuard        = %.12g\n"
      "groupRefinement  = %s\n",
      numCoeffs,
      numFloats,
      initialStep,
      minStep,
      maxPasses,
      maxMovesPerCoeff,
      stepShrink,
      rmseGuard,
      doGroupRefinement ? "ON" : "OFF"
    );

    logCoefficients(root, numCoeffs, "Initial coefficients:");
    logScore("Initial score:", best);
  }

  for (int pass = 0; pass < maxPasses && step >= minStep; pass++) {
    bool improvedThisPass = false;

    if (verboseLogging) {
      logPrint(
        "\n##################################################\n"
        "PASS %d\n"
        "##################################################\n"
        "step = %.12g\n",
        pass,
        step
      );

      logCoefficients(root, numCoeffs, "Coefficients at start of pass:");
      logScore("Best score at start of pass:", best);
    }

    for (int ci = 0; ci < numCoeffs; ci++) {
      bool improved = optimizeOneCoeff(
        root,
        ci,
        step,
        data,
        numFloats,
        best,
        evalCounter,
        maxMovesPerCoeff,
        rmseGuard,
        numCoeffs,
        verboseLogging
      );

      improvedThisPass |= improved;

      if (verboseLogging) {
        logPrint(
          "\nFinished coefficient %d\n"
          "  improvedThisCoeff = %s\n",
          ci,
          improved ? "YES" : "NO"
        );

        logCoefficients(root, numCoeffs, "Coefficients after coefficient optimization:");
        logScore("Best score after coefficient optimization:", best);
      }
    }

    if (doGroupRefinement) {
      bool groupImproved = optimizeCoeffGroupRecursive(
        root,
        0,
        numCoeffs,
        step,
        data,
        numFloats,
        best,
        evalCounter,
        rmseGuard,
        numCoeffs,
        verboseLogging
      );

      improvedThisPass |= groupImproved;

      if (verboseLogging) {
        logPrint(
          "\nFinished group refinement\n"
          "  groupImproved = %s\n",
          groupImproved ? "YES" : "NO"
        );

        logCoefficients(root, numCoeffs, "Coefficients after group refinement:");
        logScore("Best score after group refinement:", best);
      }
    }

    result.passes = pass + 1;

    if (!improvedThisPass) {
      double oldStep = step;
      step *= stepShrink;

      if (verboseLogging) {
        logPrint(
          "\nNO IMPROVEMENT THIS PASS\n"
          "  old step = %.12g\n"
          "  new step = %.12g\n",
          oldStep,
          step
        );
      }
    } else {
      if (verboseLogging) {
        logPrint(
          "\nPASS IMPROVED\n"
          "  step remains %.12g\n",
          step
        );
      }
    }
  }

  result.evals = evalCounter;
  result.finalStep = step;
  result.mae = best.mae;
  result.mse = best.mse;
  result.rmse = best.rmse;
  result.maxAbsError = best.maxAbsError;

  if (verboseLogging) {
    logPrint(
      "\n##################################################\n"
      "END COEFFICIENT OPTIMIZATION\n"
      "##################################################\n"
      "passes      = %d\n"
      "evals       = %d\n"
      "finalStep   = %.12g\n"
      "MAE         = %.12g\n"
      "MSE         = %.12g\n"
      "RMSE        = %.12g\n"
      "MaxAbsError = %.12g\n",
      result.passes,
      result.evals,
      result.finalStep,
      result.mae,
      result.mse,
      result.rmse,
      result.maxAbsError
    );

    logCoefficients(root, numCoeffs, "Final coefficients:");
  }

  return result;
}
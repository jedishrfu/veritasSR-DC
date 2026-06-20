#include <iostream>
#include <vector>
#include <cmath>
#include <cfloat>

#include "../include/ast_nodes.h"
#include "infix_parsing_code.h"

// Required because ast_nodes.h expects this global.
VarTable varTable;

struct Dataset
{
  int rows;
  double* x;
  double* y;
};

struct CoeffOptInfo
{
  Node* node;
  double step;
  int failures;

  CoeffOptInfo(Node* n, double s)
    : node(n), step(s), failures(0)
  {
  }
};

static bool isFiniteDouble(double v)
{
  return std::isfinite(v);
}

static double clampDouble(double v, double lo, double hi)
{
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static void collectCoeffNodes(Node* n, std::vector<CoeffOptInfo>& coeffs)
{
  if (!n) return;

  if (n->isCoeffNode())
  {
    double v = n->getNodeCoeff();
    double step = fabs(v) * 0.10;

    if (step < 0.1)
      step = 0.1;

    coeffs.push_back(CoeffOptInfo(n, step));
  }

  collectCoeffNodes(n->getLeftChild(), coeffs);
  collectCoeffNodes(n->getRightChild(), coeffs);
}

static double computeMSE(Node* root, Dataset& data)
{
  if (!root || data.rows <= 0 || !data.x || !data.y)
    return DBL_MAX;

  double sum = 0.0;

  for (int i = 0; i < data.rows; ++i)
  {
    varTable.setValue(0, data.x[i]);

    double pred = root->eval();

    if (!isFiniteDouble(pred))
      return DBL_MAX;

    double err = pred - data.y[i];

    if (!isFiniteDouble(err))
      return DBL_MAX;

    sum += err * err;

    if (!isFiniteDouble(sum))
      return DBL_MAX;
  }

  return sum / (double)data.rows;
}

double optimizeConstants(
  Node* root,
  Dataset& data,
  int maxIterations,
  double tolerance,
  double minCoeffValue,
  double maxCoeffValue)
{
  std::vector<CoeffOptInfo> coeffs;
  collectCoeffNodes(root, coeffs);

  if (coeffs.empty())
    return computeMSE(root, data);

  double bestMSE = computeMSE(root, data);

  const double growFactor = 1.20;
  const double shrinkFactor = 0.50;
  const double minStep = 1.0e-12;
  const int shrinkAfterFailures = 2;

  for (int iter = 0; iter < maxIterations; ++iter)
  {
    double startMSE = bestMSE;
    bool anyImproved = false;

    for (int i = 0; i < (int)coeffs.size(); ++i)
    {
      Node* c = coeffs[i].node;
      double oldValue = c->getNodeCoeff();
      double step = coeffs[i].step;

      double bestLocalValue = oldValue;
      double bestLocalMSE = bestMSE;

      double trial = clampDouble(
        oldValue + step,
        minCoeffValue,
        maxCoeffValue);

      c->setNodeCoeff(trial);
      double msePlus = computeMSE(root, data);

      if (msePlus < bestLocalMSE)
      {
        bestLocalMSE = msePlus;
        bestLocalValue = trial;
      }

      trial = clampDouble(
        oldValue - step,
        minCoeffValue,
        maxCoeffValue);

      c->setNodeCoeff(trial);
      double mseMinus = computeMSE(root, data);

      if (mseMinus < bestLocalMSE)
      {
        bestLocalMSE = mseMinus;
        bestLocalValue = trial;
      }

      if (bestLocalMSE < bestMSE)
      {
        c->setNodeCoeff(bestLocalValue);
        bestMSE = bestLocalMSE;
        coeffs[i].step *= growFactor;
        coeffs[i].failures = 0;
        anyImproved = true;
      }
      else
      {
        c->setNodeCoeff(oldValue);
        coeffs[i].failures++;

        if (coeffs[i].failures >= shrinkAfterFailures)
        {
          coeffs[i].step *= shrinkFactor;
          coeffs[i].failures = 0;

          if (coeffs[i].step < minStep)
            coeffs[i].step = minStep;
        }
      }
    }

    double improvement = startMSE - bestMSE;

    if (!anyImproved || improvement < tolerance)
      break;
  }

  return bestMSE;
}

static Dataset makeDataset()
{
  Dataset d;
  d.rows = 101;
  d.x = new double[d.rows];
  d.y = new double[d.rows];

  for (int i = 0; i < d.rows; ++i)
  {
    double x = -5.0 + 10.0 * (double)i / (double)(d.rows - 1);

    d.x[i] = x;

    // Target:
    std::cout << "TARGET: y = 3x + 2\n\n";
    d.y[i] = 3.0 * x + 2.0;
  }

  return d;
}

static void freeDataset(Dataset& d)
{
  delete[] d.x;
  delete[] d.y;

  d.x = 0;
  d.y = 0;
  d.rows = 0;
}

int main()
{
  varTable.addVar("x", 0.0);

  Dataset data = makeDataset();

  // Expression:
  //
  //   a*x + b
  //
  // Starting badly:
  //
  //   10*x - 10
  //
  Node* expr =
    Node::makeBinary(
      OP_ADD,
      Node::makeBinary(
        OP_MUL,
        Node::makeCoeffValue(10.0),
        Node::makeVariable(0)),
      Node::makeCoeffValue(-10.0));

  std::cout << "Before optimization:\n";
  std::cout << expr->toString() << "\n";
  std::cout << "MSE = " << computeMSE(expr, data) << "\n\n";

  double finalMSE = optimizeConstants(
    expr,
    data,
    10000,
    1.0e-15,
    -1.0e6,
    1.0e6);

  std::cout << "After optimization:\n";
  std::cout << expr->toString() << "\n";
  std::cout << "MSE = " << finalMSE << "\n";

  delete expr;
  freeDataset(data);

  return 0;
}
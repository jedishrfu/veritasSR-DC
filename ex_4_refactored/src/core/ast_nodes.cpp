/*
 * VeritasSR
 * Symbolic Regression and Equation Discovery System
 *
 * File: ast_nodes.cpp
 */

#include "ast_nodes.h"

double Node::eval() const
{
  switch (kind)
  {
  case NODE_VALUE:
    return cv;

  case NODE_VARIABLE:
    return varTable.getValue(vi);

  case NODE_UNARY: {
    double x = lc ? lc->eval() : 0.0;

    switch (op) {
    case OP_SIN: return sin(x);
    case OP_COS: return cos(x);
    case OP_EXP: return exp(x);
    case OP_LOG: return log(fabs(x) + 1e-9);
    default: return 0.0;
    }
  }

  case NODE_BINARY: {
    double a = lc ? lc->eval() : 0.0;
    double b = rc ? rc->eval() : 0.0;

    switch (op) {
    case OP_ADD: return a + b;
    case OP_SUB: return a - b;
    case OP_MUL: return a * b;
    case OP_DIV: return fabs(b) < 1e-12 ? 0.0 : a / b;
    default: return 0.0;
    }
  }
  }

  return 0.0;
};

std::string Node::toString() const {
  switch (kind) {
  case NODE_VALUE: {
    std::ostringstream ss;
    ss << cv;
    return ss.str();
  }

  case NODE_VARIABLE: {
    return varTable.getName(vi);
  }

  case NODE_UNARY: {
    std::string vn = lc ? lc->toString() : "?";

    switch (op) {
    case OP_SIN:
      return "sin( " + vn + " )";

    case OP_COS:
      return "cos( " + vn + " )";

    case OP_EXP:
      return "exp( " + vn + " )";

    case OP_LOG:
      return "log( " + vn + " )";

    default:
      return "?";
    }
  }

  case NODE_BINARY: {
    std::string vn = lc ? lc->toString() : "?";

    std::string cv = rc ? rc->toString() : "?";

    switch (op) {
    case OP_ADD: return "( " + vn + " + " + cv + " )";
    case OP_SUB: return "( " + vn + " - " + cv + " )";
    case OP_MUL: return "( " + vn + " * " + cv + " )";
    case OP_DIV: return "( " + vn + " / " + cv + " )";
    default: return "?";
    }
  }
  }

  return "?";
};

double randomDouble(double minVal, double maxVal)
{
  static std::random_device rd;
  static std::mt19937 gen(rd());

  std::uniform_real_distribution<double> dist(minVal, maxVal);

  return dist(gen);
};

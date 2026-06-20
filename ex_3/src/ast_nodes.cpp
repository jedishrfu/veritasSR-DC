#include <string>
#include <sstream>
#include <iomanip>

#include "../include/ast_nodes.h"
#include "../include/util_code.h"
#include "../include/var_table.h"

extern VarTable varTable;

int countNodeCoeffs(const Node* node) {
  if (node == nullptr) return 0;

  switch (node->getKind()) {
  case NODE_VALUE:
    return 1;

  case NODE_VARIABLE:
    return 0;

  case NODE_UNARY:
    return countNodeCoeffs(node->getLeftChild());

  case NODE_BINARY:
    return countNodeCoeffs(node->getLeftChild()) +
      countNodeCoeffs(node->getRightChild());
  }

  return 0;
}

void setNodeCoeffsRec(Node* node, double* ncv, int& valueNodeCount)
{
  if (node == nullptr)
    return;

  switch (node->getKind())
  {
  case NODE_VALUE:
    node->setNodeCoeff(ncv[valueNodeCount]);
    valueNodeCount++;
    return;

  case NODE_VARIABLE:
    return;

  case NODE_UNARY:
    setNodeCoeffsRec(node->getLeftChild(), ncv, valueNodeCount);
    return;

  case NODE_BINARY:
    setNodeCoeffsRec(node->getLeftChild(), ncv, valueNodeCount);
    setNodeCoeffsRec(node->getRightChild(), ncv, valueNodeCount);
    return;
  }
}

void setNodeCoeffs(Node* node, double* ncv)
{
  if (node == nullptr)
    return;

  int coeffCount = countNodeCoeffs(node);

  if (coeffCount == 0)
    return;

  if (ncv == nullptr)
  {
    logError("ERROR in setNodeCoeffs(): double* ncv is nullptr");
    return;
  }

  int valueNodeCount = 0;
  setNodeCoeffsRec(node, ncv, valueNodeCount);
}

int countNodes(const Node* node) {
  if (node == nullptr)
    return 0;

  switch (node->getKind()) {
  case NODE_VALUE:
  case NODE_VARIABLE:
    return 1;

  case NODE_UNARY:
    return 1 + countNodes(node->getLeftChild());

  case NODE_BINARY:
    return 1 +
      countNodes(node->getLeftChild()) +
      countNodes(node->getRightChild());
  }

  return 0;
}

int treeDepth(const Node* node) {
  if (node == nullptr)
    return 0;

  switch (node->getKind()) {
  case NODE_VALUE:
  case NODE_VARIABLE:
    return 1;

  case NODE_UNARY:
    return 1 + treeDepth(node->getLeftChild());

  case NODE_BINARY: {
    int ld = treeDepth(node->getLeftChild());
    int rd = treeDepth(node->getRightChild());

    return 1 + (ld > rd ? ld : rd);
  }
  }

  return 0;
};

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
    ss << std::fixed << std::setprecision(1) << cv;
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
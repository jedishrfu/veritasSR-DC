#include "../include/expr_simplifier.h"
#include "../include/expr_array.h"

#include <cmath>

static constexpr double DEFAULT_EPSILON = 1e-12;

void ExprSimplifier::simplifyExpressionArray(ExprArray& expressions,
                                             bool useCoefficientValues) {
  for (int i = 0; i < expressions.size(); i++) {
    ExprStats* es = expressions.get(i);

    if (!es || !es->n)
      continue;

    Node* oldRoot = es->n;
    es->n = ExprSimplifier::simplifyExpression(oldRoot,
                                               useCoefficientValues);

    delete oldRoot;
  }
}

Node* ExprSimplifier::simplifyExpression(const Node* root,
                                         bool useCoefficientValues) {
  if (!root)
    return nullptr;

  return simplifyNode(root, useCoefficientValues);
}

Node* ExprSimplifier::simplifyNode(const Node* root,
                                   bool useCoefficientValues) {
  if (!root)
    return nullptr;

  if (isCoeffNode(root) || isVariableNode(root))
    return root->clone();

  if (isUnaryNode(root)) {
    Node* child = simplifyNode(root->getLeftChild(), useCoefficientValues);
    return simplifyUnary(root->getOp(), child, useCoefficientValues);
  }

  if (isBinaryNode(root)) {
    Node* left = simplifyNode(root->getLeftChild(), useCoefficientValues);
    Node* right = simplifyNode(root->getRightChild(), useCoefficientValues);

    return simplifyBinary(root->getOp(),
                          left,
                          right,
                          useCoefficientValues);
  }

  return root->clone();
}

Node* ExprSimplifier::simplifyUnary(OpKind op,
                                    Node* child,
                                    bool useCoefficientValues) {
  (void)useCoefficientValues;

  if (!child)
    return nullptr;

  // Pure structural simplification:
  // -(-x) -> x
  if (op == OP_SUB && isUnaryNode(child) && child->getOp() == OP_SUB) {
    Node* grandchild = child->getLeftChild()->clone();
    delete child;
    return grandchild;
  }

  // Constant folding:
  // sin(c), cos(c), exp(c), log(c), -c
  if (isCoeffNode(child)) {
    double value = child->getNodeCoeff();

    switch (op) {
      case OP_SUB:
        delete child;
        return Node::makeCoeffValue(-value);

      case OP_SIN:
        delete child;
        return Node::makeCoeffValue(std::sin(value));

      case OP_COS:
        delete child;
        return Node::makeCoeffValue(std::cos(value));

      case OP_EXP:
        delete child;
        return Node::makeCoeffValue(std::exp(value));

      case OP_LOG:
        if (value > DEFAULT_EPSILON) {
          delete child;
          return Node::makeCoeffValue(std::log(value));
        }
        break;

      default:
        break;
    }
  }

  return Node::makeUnary(op, child);
}

Node* ExprSimplifier::simplifyBinary(OpKind op,
                                     Node* left,
                                     Node* right,
                                     bool useCoefficientValues) {
  if (!left || !right) {
    delete left;
    delete right;
    return nullptr;
  }

  // Constant folding:
  // c1 + c2 -> c3
  // c1 - c2 -> c3
  // c1 * c2 -> c3
  // c1 / c2 -> c3, if safe
  if (isCoeffNode(left) && isCoeffNode(right)) {
    double a = left->getNodeCoeff();
    double b = right->getNodeCoeff();

    switch (op) {
      case OP_ADD:
        delete left;
        delete right;
        return Node::makeCoeffValue(a + b);

      case OP_SUB:
        delete left;
        delete right;
        return Node::makeCoeffValue(a - b);

      case OP_MUL:
        delete left;
        delete right;
        return Node::makeCoeffValue(a * b);

      case OP_DIV:
        if (isSafeDenominator(b)) {
          delete left;
          delete right;
          return Node::makeCoeffValue(a / b);
        }
        break;

      default:
        break;
    }
  }

  // Pure structural simplification:
  // x - x -> 0
  if (op == OP_SUB && structurallyEqual(left, right)) {
    delete left;
    delete right;
    return Node::makeCoeffValue(0.0);
  }

  // Pure structural simplification:
  // x + x -> 2 * x
  if (op == OP_ADD && structurallyEqual(left, right)) {
    delete right;
    return Node::makeBinary(OP_MUL, Node::makeCoeffValue(2.0), left);
  }

  if (useCoefficientValues) {
    // x + 0 -> x
    if (op == OP_ADD && isZeroCoeff(right)) {
      delete right;
      return left;
    }

    // 0 + x -> x
    if (op == OP_ADD && isZeroCoeff(left)) {
      delete left;
      return right;
    }

    // x - 0 -> x
    if (op == OP_SUB && isZeroCoeff(right)) {
      delete right;
      return left;
    }

    // x * 1 -> x
    if (op == OP_MUL && isOneCoeff(right)) {
      delete right;
      return left;
    }

    // 1 * x -> x
    if (op == OP_MUL && isOneCoeff(left)) {
      delete left;
      return right;
    }

    // x * 0 -> 0
    if (op == OP_MUL && isZeroCoeff(right)) {
      delete left;
      delete right;
      return Node::makeCoeffValue(0.0);
    }

    // 0 * x -> 0
    if (op == OP_MUL && isZeroCoeff(left)) {
      delete left;
      delete right;
      return Node::makeCoeffValue(0.0);
    }

    // x / 1 -> x
    if (op == OP_DIV && isOneCoeff(right)) {
      delete right;
      return left;
    }

    // 0 / x -> 0, but avoid 0 / 0
    if (op == OP_DIV &&
        isZeroCoeff(left) &&
        !isZeroCoeff(right)) {
      delete left;
      delete right;
      return Node::makeCoeffValue(0.0);
    }
  }

  return Node::makeBinary(op, left, right);
}

bool ExprSimplifier::structurallyEqual(const Node* a, const Node* b) {
  if (a == b)
    return true;

  if (!a || !b)
    return false;

  if (a->getKind() != b->getKind())
    return false;

  if (isCoeffNode(a))
    return a->getNodeCoeff() == b->getNodeCoeff();

  if (isVariableNode(a))
    return a->getVarIndex() == b->getVarIndex();

  if (isUnaryNode(a)) {
    return a->getOp() == b->getOp() &&
           structurallyEqual(a->getLeftChild(), b->getLeftChild());
  }

  if (isBinaryNode(a)) {
    return a->getOp() == b->getOp() &&
           structurallyEqual(a->getLeftChild(), b->getLeftChild()) &&
           structurallyEqual(a->getRightChild(), b->getRightChild());
  }

  return false;
}

bool ExprSimplifier::isCoeffNode(const Node* n) {
  return n && n->getKind() == NODE_VALUE;
}

bool ExprSimplifier::isVariableNode(const Node* n) {
  return n && n->getKind() == NODE_VARIABLE;
}

bool ExprSimplifier::isUnaryNode(const Node* n) {
  return n && n->getKind() == NODE_UNARY;
}

bool ExprSimplifier::isBinaryNode(const Node* n) {
  return n && n->getKind() == NODE_BINARY;
}

bool ExprSimplifier::isZeroCoeff(const Node* n) {
  return isCoeffNode(n) &&
         std::fabs(n->getNodeCoeff()) <= DEFAULT_EPSILON;
}

bool ExprSimplifier::isOneCoeff(const Node* n) {
  return isCoeffNode(n) &&
         std::fabs(n->getNodeCoeff() - 1.0) <= DEFAULT_EPSILON;
}

bool ExprSimplifier::isSafeDenominator(double x) {
  return std::fabs(x) > DEFAULT_EPSILON;
}
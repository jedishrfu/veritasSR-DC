#include "../include/expr_simplifier.h"
#include "../include/expr_array.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

template <typename T>
void appendExact(std::string& key, const T& value) {
  key.append(reinterpret_cast<const char*>(&value), sizeof(value));
}

void appendStructuralKey(const Node* node, std::string& key) {
  const NodeKind kind = node->getKind();
  key.push_back(static_cast<char>(kind));

  if (kind == NODE_VALUE) {
    std::uint64_t bits = 0;
    const double value = node->getNodeCoeff();
    std::memcpy(&bits, &value, sizeof(bits));
    appendExact(key, bits);
    return;
  }

  if (kind == NODE_VARIABLE) {
    const int variable = node->getVarIndex();
    appendExact(key, variable);
    return;
  }

  const OpKind op = node->getOp();
  appendExact(key, op);
  appendStructuralKey(node->getLeftChild(), key);
  if (kind == NODE_BINARY)
    appendStructuralKey(node->getRightChild(), key);
}

std::string structuralKey(const Node* node) {
  std::string key;
  appendStructuralKey(node, key);
  return key;
}

struct SignedTerm {
  const Node* node;
  double sign;
};

void collectRawAddTerms(const Node* node, double sign,
                        std::vector<SignedTerm>& terms) {
  if (node->getKind() == NODE_BINARY && node->getOp() == OP_ADD) {
    collectRawAddTerms(node->getLeftChild(), sign, terms);
    collectRawAddTerms(node->getRightChild(), sign, terms);
    return;
  }
  if (node->getKind() == NODE_BINARY && node->getOp() == OP_SUB) {
    collectRawAddTerms(node->getLeftChild(), sign, terms);
    collectRawAddTerms(node->getRightChild(), -sign, terms);
    return;
  }
  if (node->getKind() == NODE_UNARY && node->getOp() == OP_SUB) {
    collectRawAddTerms(node->getLeftChild(), -sign, terms);
    return;
  }

  terms.push_back({node, sign});
}

void collectRawMulFactors(const Node* node,
                          std::vector<const Node*>& factors) {
  if (node->getKind() == NODE_BINARY && node->getOp() == OP_MUL) {
    collectRawMulFactors(node->getLeftChild(), factors);
    collectRawMulFactors(node->getRightChild(), factors);
    return;
  }
  factors.push_back(node);
}

Node* buildBalancedTree(OpKind op, std::vector<Node*>& nodes,
                        std::size_t begin, std::size_t end) {
  if (end - begin == 1)
    return nodes[begin];

  const std::size_t middle = begin + (end - begin) / 2;
  return Node::makeBinary(
      op,
      buildBalancedTree(op, nodes, begin, middle),
      buildBalancedTree(op, nodes, middle, end));
}

Node* makeProduct(double coefficient, Node* symbolic,
                  bool useCoefficientValues) {
  if (useCoefficientValues && coefficient == 1.0)
    return symbolic;
  return Node::makeBinary(OP_MUL, Node::makeCoeffValue(coefficient), symbolic);
}

} // namespace

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

    if (es->ns) {
      es->ns->nodeCount = countNodes(es->n);
      es->ns->depth = treeDepth(es->n);
    }
  }
}

Node* ExprSimplifier::simplifyExpression(const Node* root,
                                         bool useCoefficientValues) {
  if (!root)
    return nullptr;

  Node* current = root->clone();

  static constexpr int MAX_SIMPLIFY_PASSES = 64;
  for (int pass = 0; pass < MAX_SIMPLIFY_PASSES; ++pass) {
    Node* next = simplifyNode(current, useCoefficientValues);
    if (structurallyEqual(current, next)) {
      delete current;
      return next;
    }

    delete current;
    current = next;
  }

  return current;
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
    // Flatten a maximal addition region before recursing into it. This avoids
    // repeatedly cloning growing addition prefixes for skewed large trees.
    if (root->getOp() == OP_ADD || root->getOp() == OP_SUB)
      return simplifyAdd(root, useCoefficientValues);
    if (root->getOp() == OP_MUL)
      return simplifyMul(root, useCoefficientValues);

    Node* left = simplifyNode(root->getLeftChild(), useCoefficientValues);
    Node* right = simplifyNode(root->getRightChild(), useCoefficientValues);

    return simplifyBinary(root->getOp(),
                          left,
                          right,
                          useCoefficientValues);
  }

  return root->clone();
}

Node* ExprSimplifier::simplifyAdd(const Node* root,
                                  bool useCoefficientValues) {
  std::vector<SignedTerm> rawTerms;
  collectRawAddTerms(root, 1.0, rawTerms);

  struct CoefficientTerm {
    Node* symbolic;
    double coefficient;
  };
  std::vector<CoefficientTerm> terms;
  double constantSum = 0.0;

  for (const SignedTerm& rawTerm : rawTerms) {
    Node* term = simplifyNode(rawTerm.node, useCoefficientValues);
    if (isCoeffNode(term)) {
      constantSum += rawTerm.sign * term->getNodeCoeff();
      delete term;
    }
    else {
      double coefficient = rawTerm.sign;
      Node* symbolic = term;
      if (isBinaryNode(term) && term->getOp() == OP_MUL &&
          isCoeffNode(term->getLeftChild())) {
        coefficient *= term->getLeftChild()->getNodeCoeff();
        symbolic = term->getRightChild()->clone();
        delete term;
      }
      terms.push_back({symbolic, coefficient});
    }
  }

  struct TermGroup {
    Node* term;
    double coefficient;
    std::string key;
  };

  std::vector<TermGroup> groups;
  std::unordered_map<std::string, std::size_t> groupByKey;
  for (const CoefficientTerm& term : terms) {
    const std::string key = structuralKey(term.symbolic);
    const auto found = groupByKey.find(key);
    if (found == groupByKey.end()) {
      groupByKey.emplace(key, groups.size());
      groups.push_back({term.symbolic, term.coefficient, key});
    }
    else {
      groups[found->second].coefficient += term.coefficient;
      delete term.symbolic;
    }
  }
  std::sort(groups.begin(), groups.end(),
            [](const TermGroup& a, const TermGroup& b) {
              return a.key < b.key;
            });

  std::vector<Node*> combinedTerms;
  for (const TermGroup& group : groups) {
    if (useCoefficientValues && group.coefficient == 0.0) {
      delete group.term;
      continue;
    }
    combinedTerms.push_back(makeProduct(group.coefficient, group.term,
                                        useCoefficientValues));
  }

  if (!useCoefficientValues ||
      constantSum != 0.0 ||
      combinedTerms.empty()) {
    combinedTerms.push_back(Node::makeCoeffValue(constantSum));
  }

  return buildBalancedTree(OP_ADD, combinedTerms, 0, combinedTerms.size());
}

Node* ExprSimplifier::simplifyMul(const Node* root,
                                  bool useCoefficientValues) {
  std::vector<const Node*> rawFactors;
  collectRawMulFactors(root, rawFactors);

  double coefficient = 1.0;
  struct Factor {
    Node* node;
    std::string key;
  };
  std::vector<Factor> factors;
  for (const Node* rawFactor : rawFactors) {
    Node* factor = simplifyNode(rawFactor, useCoefficientValues);
    if (isCoeffNode(factor)) {
      coefficient *= factor->getNodeCoeff();
      delete factor;
    }
    else {
      factors.push_back({factor, structuralKey(factor)});
    }
  }

  if (useCoefficientValues && coefficient == 0.0) {
    for (const Factor& factor : factors)
      delete factor.node;
    return Node::makeCoeffValue(0.0);
  }

  std::sort(factors.begin(), factors.end(),
            [](const Factor& a, const Factor& b) { return a.key < b.key; });
  std::vector<Node*> symbolicFactors;
  for (std::size_t i = 0; i < factors.size();) {
    std::size_t end = i + 1;
    while (end < factors.size() && factors[end].key == factors[i].key) {
      delete factors[end].node;
      ++end;
    }
    const std::size_t occurrences = end - i;
    if (occurrences == 1) {
      symbolicFactors.push_back(factors[i].node);
    }
    else {
      symbolicFactors.push_back(Node::makeBinary(
          OP_POW, factors[i].node,
          Node::makeCoeffValue(static_cast<double>(occurrences))));
    }
    i = end;
  }

  if (symbolicFactors.empty())
    return Node::makeCoeffValue(coefficient);

  Node* symbolic = buildBalancedTree(OP_MUL, symbolicFactors, 0,
                                     symbolicFactors.size());
  return makeProduct(coefficient, symbolic, useCoefficientValues);
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
        if (value > 0.0) {
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

      case OP_POW: {
        const double value = std::pow(a, b);
        if (std::isfinite(value)) {
          delete left;
          delete right;
          return Node::makeCoeffValue(value);
        }
        break;
      }

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
    if (op == OP_POW && isOneCoeff(right)) {
      delete right;
      return left;
    }

    if (op == OP_POW && isZeroCoeff(right)) {
      delete left;
      delete right;
      return Node::makeCoeffValue(1.0);
    }

    if (op == OP_POW && isOneCoeff(left)) {
      delete left;
      delete right;
      return Node::makeCoeffValue(1.0);
    }

    if (op == OP_POW && isZeroCoeff(left) && isCoeffNode(right) &&
        right->getNodeCoeff() > 0.0) {
      delete left;
      delete right;
      return Node::makeCoeffValue(0.0);
    }

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
  return isCoeffNode(n) && n->getNodeCoeff() == 0.0;
}

bool ExprSimplifier::isOneCoeff(const Node* n) {
  return isCoeffNode(n) && n->getNodeCoeff() == 1.0;
}

bool ExprSimplifier::isSafeDenominator(double x) {
  return x != 0.0;
}

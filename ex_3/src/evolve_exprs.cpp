#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#include <string>
#include <vector>
#include <set>
#include <sstream>
#include <cmath>

#include "../include/ast_nodes.h"
#include "../include/ast_nodestats.h"
#include "../include/expr_array.h"
#include "../include/util_code.h"

static OpKind randomUnaryOp() {
  switch (rand() % 4) {
  case 0: return OP_SIN;
  case 1: return OP_COS;
  case 2: return OP_EXP;
  case 3: return OP_LOG;
  }

  return OP_SIN;
}

static OpKind randomBinaryOp() {
  switch (rand() % 4) {
  case 0: return OP_ADD;
  case 1: return OP_SUB;
  case 2: return OP_MUL;
  case 3: return OP_DIV;
  }

  return OP_ADD;
}

static int countOpNodes(const Node* n) {
  if (!n) return 0;

  if (n->getKind() == NODE_UNARY)
    return 1 + countOpNodes(n->getLeftChild());

  if (n->getKind() == NODE_BINARY)
    return 1 + countOpNodes(n->getLeftChild()) + countOpNodes(n->getRightChild());

  return 0;
}

static Node* mutateRandomOperatorRec(const Node* src, int target, int& seen) {
  if (!src) return NULL;

  NodeKind kind = src->getKind();

  if (kind == NODE_VALUE)
    return Node::makeCoeffValue(src->getNodeCoeff());

  if (kind == NODE_VARIABLE)
    return src->clone();

  if (kind == NODE_UNARY) {
    OpKind op = src->getOp();

    if (seen == target) {
      OpKind newOp = op;

      while (newOp == op)
        newOp = randomUnaryOp();

      op = newOp;
    }

    seen++;

    return Node::makeUnary(
      op,
      mutateRandomOperatorRec(src->getLeftChild(), target, seen)
    );
  }

  if (kind == NODE_BINARY) {
    OpKind op = src->getOp();

    if (seen == target) {
      OpKind newOp = op;

      while (newOp == op)
        newOp = randomBinaryOp();

      op = newOp;
    }

    seen++;

    return Node::makeBinary(
      op,
      mutateRandomOperatorRec(src->getLeftChild(), target, seen),
      mutateRandomOperatorRec(src->getRightChild(), target, seen)
    );
  }

  return src->clone();
}

static Node* mutateRandomOperator(const Node* src) {
  int opCount = countOpNodes(src);

  if (opCount == 0)
    return src->clone();

  int target = rand() % opCount;
  int seen = 0;

  return mutateRandomOperatorRec(src, target, seen);
}

static Node* deleteRandomNodeRec(const Node* src, int target, int& seen) {
  if (!src) return NULL;

  NodeKind kind = src->getKind();

  if (kind == NODE_VALUE)
    return Node::makeCoeffValue(src->getNodeCoeff());

  if (kind == NODE_VARIABLE)
    return src->clone();

  if (kind == NODE_UNARY) {
    if (seen == target)
      return src->getLeftChild() ? src->getLeftChild()->clone() : src->clone();

    seen++;

    return Node::makeUnary(
      src->getOp(),
      deleteRandomNodeRec(src->getLeftChild(), target, seen)
    );
  }

  if (kind == NODE_BINARY) {
    if (seen == target) {
      if (rand() % 2 == 0 && src->getLeftChild())
        return src->getLeftChild()->clone();

      if (src->getRightChild())
        return src->getRightChild()->clone();

      return src->clone();
    }

    seen++;

    return Node::makeBinary(
      src->getOp(),
      deleteRandomNodeRec(src->getLeftChild(), target, seen),
      deleteRandomNodeRec(src->getRightChild(), target, seen)
    );
  }

  return src->clone();
}

static Node* deleteRandomNode(const Node* src) {
  int opCount = countOpNodes(src);

  if (opCount == 0)
    return src->clone();

  int target = rand() % opCount;
  int seen = 0;

  return deleteRandomNodeRec(src, target, seen);
}

static int countAllNodes(const Node* n) {
  if (!n) return 0;

  return 1
    + countAllNodes(n->getLeftChild())
    + countAllNodes(n->getRightChild());
}

static int countVariableNodes(const Node* n) {
  if (!n) return 0;

  int count = 0;

  if (n->getKind() == NODE_VARIABLE)
    count++;

  count += countVariableNodes(n->getLeftChild());
  count += countVariableNodes(n->getRightChild());

  return count;
}

static Node* cloneSubtreeAt(const Node* src, int target, int& seen) {
  if (!src) return NULL;

  if (seen == target)
    return src->clone();

  seen++;

  Node* leftResult = cloneSubtreeAt(src->getLeftChild(), target, seen);
  if (leftResult) return leftResult;

  Node* rightResult = cloneSubtreeAt(src->getRightChild(), target, seen);
  if (rightResult) return rightResult;

  return NULL;
}

static Node* cloneReplacingSubtreeAt(
  const Node* src,
  int target,
  int& seen,
  const Node* replacement) {
  if (!src) return NULL;

  if (seen == target) {
    seen++;
    return replacement ? replacement->clone() : NULL;
  }

  seen++;

  NodeKind kind = src->getKind();

  if (kind == NODE_VALUE)
    return Node::makeCoeffValue(src->getNodeCoeff());

  if (kind == NODE_VARIABLE)
    return src->clone();

  if (kind == NODE_UNARY) {
    return Node::makeUnary(
      src->getOp(),
      cloneReplacingSubtreeAt(src->getLeftChild(), target, seen, replacement)
    );
  }

  if (kind == NODE_BINARY) {
    return Node::makeBinary(
      src->getOp(),
      cloneReplacingSubtreeAt(src->getLeftChild(), target, seen, replacement),
      cloneReplacingSubtreeAt(src->getRightChild(), target, seen, replacement)
    );
  }

  return src->clone();
}

static Node* crossoverSubtrees(const Node* a, const Node* b) {
  if (!a || !b) return NULL;

  int countA = countAllNodes(a);
  int countB = countAllNodes(b);

  if (countA == 0 || countB == 0)
    return a->clone();

  int cutA = rand() % countA;
  int cutB = rand() % countB;

  int seenB = 0;
  Node* donor = cloneSubtreeAt(b, cutB, seenB);

  if (!donor)
    return a->clone();

  int seenA = 0;
  Node* child = cloneReplacingSubtreeAt(a, cutA, seenA, donor);

  delete donor;

  return child;
}

static Node* makeAffineVariable(int varIndex) {
  double a = randomDouble(-10.0, 10.0);
  double b = randomDouble(-10.0, 10.0);

  return Node::makeBinary(
    OP_ADD,
    Node::makeBinary(
      OP_MUL,
      Node::makeCoeffValue(a),
      Node::makeVariable(varIndex)
    ),
    Node::makeCoeffValue(b)
  );
}

static Node* mutateRandomVariableToAffineRec(
  const Node* src,
  int target,
  int& seen) {
  if (!src) return NULL;

  NodeKind kind = src->getKind();

  if (kind == NODE_VALUE)
    return Node::makeCoeffValue(src->getNodeCoeff());

  if (kind == NODE_VARIABLE) {
    if (seen == target) {
      seen++;
      return makeAffineVariable(src->getVarIndex());
    }

    seen++;
    return src->clone();
  }

  if (kind == NODE_UNARY) {
    return Node::makeUnary(
      src->getOp(),
      mutateRandomVariableToAffineRec(src->getLeftChild(), target, seen)
    );
  }

  if (kind == NODE_BINARY) {
    return Node::makeBinary(
      src->getOp(),
      mutateRandomVariableToAffineRec(src->getLeftChild(), target, seen),
      mutateRandomVariableToAffineRec(src->getRightChild(), target, seen)
    );
  }

  return src->clone();
}

static Node* mutateRandomVariableToAffine(const Node* src) {
  int varCount = countVariableNodes(src);

  if (varCount == 0)
    return src ? src->clone() : NULL;

  int target = rand() % varCount;
  int seen = 0;

  return mutateRandomVariableToAffineRec(src, target, seen);
}

Node* mutateRandomCoeff(Node* parent) {
  if (parent == nullptr)
    return nullptr;

  Node* root = parent->clone();

  std::vector<Node*> coeffNodes;

  std::function<void(Node*)> collectCoeffs =
    [&](Node* node) {
    if (node == nullptr)
      return;

    if (node->getKind() == NODE_VALUE)
      coeffNodes.push_back(node);

    collectCoeffs(node->getLeftChild());
    collectCoeffs(node->getRightChild());
  };

  collectCoeffs(root);

  if (coeffNodes.empty())
    return root;

  int idx = std::rand() % coeffNodes.size();

  Node* coeff = coeffNodes[idx];

  double oldValue = coeff->getNodeCoeff();

  if (std::fabs(oldValue) < 1.0e-12) {
    coeff->setNodeCoeff(randomDouble(-10.0, 10.0));
  }
  else {
    coeff->setNodeCoeff(
      oldValue * randomDouble(0.8, 1.2));
  }

  return root;
}

int bucket(int n) {
  if (n <= 150)
    return 1;

  return ((n - 151) / 100) + 2;
}

ExprArray* evolveExpressions(ExprArray* input) {
  ExprArray* result = new ExprArray();
  std::set<std::string> seen;

  if (!input || input->size() == 0)
    return result;

  enum MutationType {
    MUT_CONST = 0,
    MUT_UNARY,
    MUT_BINARY,
    MUT_CHANGE_OP,
    MUT_DELETE_NODE,
    MUT_VARIABLE_AFFINE,
    MUT_CROSSOVER
  };

  const int NUM_CASES = 7;

  int step = bucket(input->size() / 100);

  for (int i = 0; i < input->size(); i += step) {
    if (!input->items[i] || !input->items[i]->n)
      continue;

    Node* parent = input->items[i]->n;

    addUniqueExprStats(result, input->items[i], seen);

    Node* newTree = NULL;

    int choice = rand() % NUM_CASES;

    switch (choice) {
    case MUT_CONST: {
      if (countNodeCoeffs(parent) > 0)
        newTree = mutateRandomCoeff(parent);
      else
        newTree = mutateRandomVariableToAffine(parent);

      break;
    }

    case MUT_UNARY: {
      newTree = Node::makeUnary(
        randomUnaryOp(),
        parent->clone()
      );

      break;
    }

    case MUT_BINARY: {
      int j = rand() % input->size();

      if (j == i && input->size() > 1)
        j = (j + 1) % input->size();

      if (!input->items[j] || !input->items[j]->n)
        break;

      Node* other = input->items[j]->n;
      OpKind op = randomBinaryOp();

      if (rand() % 2 == 0) {
        newTree = Node::makeBinary(
          op,
          parent->clone(),
          other->clone()
        );
      }
      else {
        newTree = Node::makeBinary(
          op,
          other->clone(),
          parent->clone()
        );
      }

      break;
    }

    case MUT_CHANGE_OP: {
      newTree = mutateRandomOperator(parent);
      break;
    }

    case MUT_DELETE_NODE: {
      newTree = deleteRandomNode(parent);
      break;
    }

    case MUT_VARIABLE_AFFINE: {
      newTree = mutateRandomVariableToAffine(parent);
      break;
    }

    case MUT_CROSSOVER: {
      int j = rand() % input->size();

      if (j == i && input->size() > 1)
        j = (j + 1) % input->size();

      if (!input->items[j] || !input->items[j]->n)
        break;

      newTree = crossoverSubtrees(parent, input->items[j]->n);
      break;
    }

    default:
      break;
    }

    if (newTree)
      addUniqueTree(result, newTree, seen);
  }

  return result;
}

void resetNodeCoeffs(Node* node) {
  if (!node) return;

  if (node->getKind() == NODE_VALUE)
    node->setNodeCoeff(NAN);

  resetNodeCoeffs(node->getLeftChild());
  resetNodeCoeffs(node->getRightChild());
}

ExprArray* filterPool(
  ExprArray* input,
  double cutoffScore) {
  ExprArray* result = new ExprArray();

  for (int i = 0; i < input->size(); i++) {
    ExprStats* es = input->items[i];

    if (!es || !es->ns)
      continue;

    double mse = es->ns->mse;

    if (std::isfinite(mse) && mse < cutoffScore) {
      result->add(es->clone());
    }
  }

  return result;
}

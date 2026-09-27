#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <algorithm>

#include <string>
#include <vector>
#include <set>
#include <sstream>
#include <cmath>
#include <array>

#include "../include/ast_nodes.h"
#include "../include/ast_nodestats.h"
#include "../include/expr_array.h"
#include "../include/expr_simplifier.h"
#include "../include/util_code.h"

namespace {

enum class EvolutionOperation {
  RotateTree,
  AddUnaryNode,
  AddBinaryNode,
  DeleteNode,
  MutateOperator,
  DoNothing,
  SwapOperands,
  MutateConstant,
  InsertAffine,
  Simplify,
  Randomize,
  Optimize,
  Crossover,
  Count
};

struct OperationStats {
  unsigned long long attempts = 0;
  unsigned long long produced = 0;
};

std::array<OperationStats,
           static_cast<std::size_t>(EvolutionOperation::Count)> operationStats;

const char* operationName(EvolutionOperation operation) {
  static const char* names[] = {
    "rotate_tree", "add_unary_node", "add_binary_node", "delete_node",
    "mutate_operator", "do_nothing", "swap_operands", "mutate_constant",
    "insert_affine", "simplify", "randomize", "optimize", "crossover"
  };
  return names[static_cast<std::size_t>(operation)];
}

EvolutionOperation selectMutation(const EvolutionRates& rates) {
  const std::array<double, 12> weights = {
    rates.rotateTree, rates.addUnaryNode, rates.addBinaryNode,
    rates.deleteNode, rates.mutateOperator, rates.doNothing,
    rates.swapOperands, rates.mutateConstant, rates.insertAffine,
    rates.simplify, rates.randomize, rates.optimize
  };
  double target = randomDouble(0.0, rates.totalMutationWeight());
  for (std::size_t i = 0; i < weights.size(); ++i) {
    target -= weights[i];
    if (target <= 0.0)
      return static_cast<EvolutionOperation>(i);
  }
  return EvolutionOperation::Optimize;
}

} // namespace

void logEvolutionOperatorStats() {
  logNote("Evolution operator outcomes:");
  for (std::size_t i = 0; i < operationStats.size(); ++i) {
    const OperationStats& stats = operationStats[i];
    if (stats.attempts == 0) continue;
    logNote("  %-20s attempts=%llu produced=%llu", operationName(
            static_cast<EvolutionOperation>(i)), stats.attempts, stats.produced);
  }
}

static OpKind randomUnaryOp() {
  switch (randomInt(3)) {
  case 0: return OP_SIN;
  case 1: return OP_EXP;
  case 2: return OP_LOG;
  }

  return OP_SIN;
}

static OpKind randomBinaryOp() {
  switch (randomInt(4)) {
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

  int target = randomInt(opCount);
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
      if (randomInt(2) == 0 && src->getLeftChild())
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

  int target = randomInt(opCount);
  int seen = 0;

  return deleteRandomNodeRec(src, target, seen);
}

static int countAllNodes(const Node* n) {
  if (!n) return 0;

  return 1
    + countAllNodes(n->getLeftChild())
    + countAllNodes(n->getRightChild());
}

static bool isAssociativeNode(const Node* node) {
  return node && node->getKind() == NODE_BINARY &&
      (node->getOp() == OP_ADD || node->getOp() == OP_MUL);
}

static int countRotatableNodes(const Node* node) {
  if (!node) return 0;
  int count = 0;
  if (isAssociativeNode(node)) {
    const OpKind op = node->getOp();
    const Node* left = node->getLeftChild();
    const Node* right = node->getRightChild();
    if ((isAssociativeNode(left) && left->getOp() == op) ||
        (isAssociativeNode(right) && right->getOp() == op))
      count = 1;
  }
  return count + countRotatableNodes(node->getLeftChild()) +
      countRotatableNodes(node->getRightChild());
}

static Node* rotateTreeRec(const Node* node, int target, int& seen) {
  if (!node) return nullptr;
  if (isAssociativeNode(node)) {
    const OpKind op = node->getOp();
    const Node* left = node->getLeftChild();
    const Node* right = node->getRightChild();
    const bool rotateLeft = isAssociativeNode(left) && left->getOp() == op;
    const bool rotateRight = isAssociativeNode(right) && right->getOp() == op;
    if (rotateLeft || rotateRight) {
      if (seen == target) {
        ++seen;
        if (rotateLeft) {
          return Node::makeBinary(op, left->getLeftChild()->clone(),
              Node::makeBinary(op, left->getRightChild()->clone(), right->clone()));
        }
        return Node::makeBinary(op,
            Node::makeBinary(op, left->clone(), right->getLeftChild()->clone()),
            right->getRightChild()->clone());
      }
      ++seen;
    }
  }
  if (node->getKind() == NODE_UNARY)
    return Node::makeUnary(node->getOp(),
        rotateTreeRec(node->getLeftChild(), target, seen));
  if (node->getKind() == NODE_BINARY)
    return Node::makeBinary(node->getOp(),
        rotateTreeRec(node->getLeftChild(), target, seen),
        rotateTreeRec(node->getRightChild(), target, seen));
  return node->clone();
}

static Node* rotateRandomTree(const Node* node) {
  const int count = countRotatableNodes(node);
  if (count == 0) return node ? node->clone() : nullptr;
  int seen = 0;
  return rotateTreeRec(node, randomInt(count), seen);
}

static Node* swapOperandsRec(const Node* node, int target, int& seen) {
  if (!node) return nullptr;
  if (node->getKind() == NODE_BINARY) {
    if (seen++ == target)
      return Node::makeBinary(node->getOp(), node->getRightChild()->clone(),
                              node->getLeftChild()->clone());
    return Node::makeBinary(node->getOp(),
        swapOperandsRec(node->getLeftChild(), target, seen),
        swapOperandsRec(node->getRightChild(), target, seen));
  }
  if (node->getKind() == NODE_UNARY)
    return Node::makeUnary(node->getOp(),
        swapOperandsRec(node->getLeftChild(), target, seen));
  return node->clone();
}

static Node* swapRandomOperands(const Node* node) {
  int binaryCount = 0;
  std::function<void(const Node*)> count = [&](const Node* current) {
    if (!current) return;
    if (current->getKind() == NODE_BINARY) ++binaryCount;
    count(current->getLeftChild());
    count(current->getRightChild());
  };
  count(node);
  if (binaryCount == 0) return node ? node->clone() : nullptr;
  int seen = 0;
  return swapOperandsRec(node, randomInt(binaryCount), seen);
}

static Node* makeRandomExpression(int varID) {
  Node* variable = Node::makeVariable(varID);
  if (randomInt(2) == 0)
    return Node::makeUnary(randomUnaryOp(), variable);
  return Node::makeBinary(randomBinaryOp(), variable,
                          Node::makeCoeffValue(randomDouble(-10.0, 10.0)));
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

  int cutA = randomInt(countA);
  int cutB = randomInt(countB);

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

  int target = randomInt(varCount);
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

  int idx = randomInt(static_cast<int>(coeffNodes.size()));

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

ExprArray* evolveExpressions(
  ExprArray* input,
  int blockID,
  int genID,
  int stepID,
  int varID,
  const Options& opts,
  float* data) {
  ExprArray* result = new ExprArray();
  std::set<std::string> seen;

  if (!input || input->size() == 0)
    return result;

  int step = bucket(input->size() / 100);

  for (int i = 0; i < input->size(); i += step) {
    if (!input->items[i] || !input->items[i]->n)
      continue;

    Node* parent = input->items[i]->n;

    addUniqueExprStats(result, input->items[i], seen);

    Node* newTree = NULL;

    const bool useCrossover =
        randomDouble(0.0, 1.0) < opts.evolutionRates.crossoverProbability;
    const EvolutionOperation operation = useCrossover
        ? EvolutionOperation::Crossover
        : selectMutation(opts.evolutionRates);
    OperationStats& stats = operationStats[static_cast<std::size_t>(operation)];
    ++stats.attempts;

    switch (operation) {
    case EvolutionOperation::MutateConstant: {
      if (countNodeCoeffs(parent) > 0)
        newTree = mutateRandomCoeff(parent);
      else
        newTree = mutateRandomVariableToAffine(parent);

      break;
    }

    case EvolutionOperation::AddUnaryNode: {
      newTree = Node::makeUnary(
        randomUnaryOp(),
        parent->clone()
      );

      break;
    }

    case EvolutionOperation::AddBinaryNode: {
      int j = randomInt(input->size());

      if (j == i && input->size() > 1)
        j = (j + 1) % input->size();

      if (!input->items[j] || !input->items[j]->n)
        break;

      Node* other = input->items[j]->n;
      OpKind op = randomBinaryOp();

      if (randomInt(2) == 0) {
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

    case EvolutionOperation::MutateOperator: {
      newTree = mutateRandomOperator(parent);
      break;
    }

    case EvolutionOperation::DeleteNode: {
      newTree = deleteRandomNode(parent);
      break;
    }

    case EvolutionOperation::InsertAffine: {
      newTree = mutateRandomVariableToAffine(parent);
      break;
    }

    case EvolutionOperation::RotateTree:
      newTree = rotateRandomTree(parent);
      break;

    case EvolutionOperation::SwapOperands:
      newTree = swapRandomOperands(parent);
      break;

    case EvolutionOperation::DoNothing:
      newTree = parent->clone();
      break;

    case EvolutionOperation::Simplify:
      newTree = ExprSimplifier::simplifyExpression(parent, true);
      break;

    case EvolutionOperation::Randomize:
      newTree = makeRandomExpression(varID);
      break;

    case EvolutionOperation::Optimize:
      newTree = parent->clone();
      break;

    case EvolutionOperation::Crossover: {
      int j = randomInt(input->size());

      if (j == i && input->size() > 1)
        j = (j + 1) % input->size();

      if (!input->items[j] || !input->items[j]->n)
        break;

      newTree = crossoverSubtrees(parent, input->items[j]->n);
      break;
    }

    case EvolutionOperation::Count:
    default:
      break;
    }

    if (newTree) {
      optimize_NodeCoeffs_HillClimbing_Search(
        blockID,
        genID,
        stepID,
        varID,
        opts,
        newTree,
        data,
        1.0,
        1000,
        0);

      if (addUniqueTree(result, newTree, seen))
        ++stats.produced;
    }
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

ExprArray* filterPool(ExprArray* input) {
  ExprArray* result = new ExprArray();

  if (!input || input->size() == 0)
    return result;

  std::vector<ExprStats*> valid;
  int rejectedNonFinite = 0;

  const auto hasOnlyFiniteCoefficients = [](const Node* root) {
    std::vector<const Node*> pending;
    pending.push_back(root);

    while (!pending.empty()) {
      const Node* node = pending.back();
      pending.pop_back();

      if (!node)
        continue;

      if (node->getKind() == NODE_VALUE &&
          !std::isfinite(node->getNodeCoeff())) {
        return false;
      }

      pending.push_back(node->getLeftChild());
      pending.push_back(node->getRightChild());
    }

    return true;
  };

  // Reject expressions containing non-finite coefficients or that produced
  // any non-finite value or score.
  for (int i = 0; i < input->size(); i++) {
    ExprStats* es = input->items[i];

    if (!es || !es->n || !es->ns)
      continue;

    const NodeStats& stats = *es->ns;
    const bool finite =
        hasOnlyFiniteCoefficients(es->n) &&
        stats.numNaN == 0 &&
        stats.numInfinity == 0 &&
        std::isfinite(stats.maxAbsError) &&
        std::isfinite(stats.mae) &&
        std::isfinite(stats.mse) &&
        std::isfinite(stats.rmse) &&
        std::isfinite(stats.psnr);

    if (finite)
      valid.push_back(es);
    else
      rejectedNonFinite++;
  }

  if (rejectedNonFinite > 0)
    logPrint("Rejected %d expressions with NaN or infinity values/scores.",
             rejectedNonFinite);

  if (valid.empty())
    return result;

  // Sort by increasing maxAbsError (best first).
  std::sort(valid.begin(), valid.end(),
            [](const ExprStats* a, const ExprStats* b) {
                return a->ns->maxAbsError < b->ns->maxAbsError;
            });

  // Keep the best half
  size_t keep = valid.size() / 2;

  // Keep one expression when there is exactly one valid candidate.
  if (keep==0) keep = 1;

  for (size_t i = 0; i < keep; i++) {
    result->add(valid[i]->clone());
  }

  return result;
}

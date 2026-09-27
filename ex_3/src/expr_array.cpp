#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <set>

#include "../include/expr_array.h"

#include "../include/util_code.h"

namespace {

template <typename T>
void appendExact(std::string& key, const T& value) {
  key.append(reinterpret_cast<const char*>(&value), sizeof(value));
}

void appendStructuralKey(const Node* node, std::string& key) {
  if (!node) {
    key.push_back('N');
    return;
  }

  const NodeKind kind = node->getKind();
  key.push_back(static_cast<char>(kind));

  switch (kind) {
    case NODE_VALUE: {
      std::uint64_t bits = 0;
      const double value = node->getNodeCoeff();
      static_assert(sizeof(bits) == sizeof(value),
                    "double must fit in the structural key");
      std::memcpy(&bits, &value, sizeof(bits));
      appendExact(key, bits);
      return;
    }

    case NODE_VARIABLE: {
      const int variable = node->getVarIndex();
      appendExact(key, variable);
      return;
    }

    case NODE_UNARY:
    case NODE_BINARY: {
      const OpKind op = node->getOp();
      appendExact(key, op);
      appendStructuralKey(node->getLeftChild(), key);
      appendStructuralKey(node->getRightChild(), key);
      return;
    }
  }
}

std::string structuralKey(const Node* node) {
  std::string key;
  appendStructuralKey(node, key);
  return key;
}

bool isBetterDuplicate(const ExprStats* candidate,
                       const ExprStats* incumbent) {
  const bool candidateFinite = candidate->ns &&
      std::isfinite(candidate->ns->maxAbsError);
  const bool incumbentFinite = incumbent->ns &&
      std::isfinite(incumbent->ns->maxAbsError);

  if (candidateFinite != incumbentFinite)
    return candidateFinite;

  if (candidateFinite &&
      candidate->ns->maxAbsError != incumbent->ns->maxAbsError) {
    return candidate->ns->maxAbsError < incumbent->ns->maxAbsError;
  }

  return countNodes(candidate->n) < countNodes(incumbent->n);
}

} // namespace

bool addUniqueTree(
  ExprArray* result,
  Node* tree,
  std::set<std::string>& seen) {

  //logPrint("- checking expression: %s",tree->toString().c_str());
  if (!result || !tree)
    return false;

  std::string key = structuralKey(tree);

  if (seen.find(key) != seen.end()) {
    delete tree;
    return false;
  }

  //logPrint("- expression unique: %s",tree->toString().c_str());
  seen.insert(key);

  auto ns = new NodeStats();
  auto es = new ExprStats(tree, ns);
  //logPrint("- adding es->n expression: %s",es->n->toString().c_str());
  result->add(es);

  //logPrint("- adding expression: %s",es->n->toString().c_str());

  return true;
}

bool addUniqueExprStats(
  ExprArray* result,
  ExprStats* src,
  std::set<std::string>& seen) {
  if (!src || !src->n)
    return false;

  return addUniqueTree(result, src->n->clone(), seen);
}

ExprArray* removeDuplicateExpressions(const ExprArray* input) {
  auto* result = new ExprArray();

  if (!input)
    return result;

  std::map<std::string, size_t> positions;
  int removed = 0;

  for (const ExprStats* es : input->items) {
    if (!es || !es->n)
      continue;

    const std::string key = structuralKey(es->n);
    const auto found = positions.find(key);

    if (found == positions.end()) {
      positions.emplace(key, result->items.size());
      result->add(es->clone());
      continue;
    }

    ExprStats*& incumbent = result->items[found->second];
    if (isBetterDuplicate(es, incumbent)) {
      delete incumbent;
      incumbent = es->clone();
    }
    removed++;
  }

  if (removed > 0)
    logPrint("Removed %d duplicate expressions from the pool.", removed);

  return result;
}

// ExprArray* filterPool(
//   ExprArray* input,
//   double cutoffScore)
// {
//   ExprArray* result = new ExprArray();
//
//   for (int i = 0; i < input->size(); i++)
//   {
//     ExprStats* es = input->items[i];
//
//     if (!es || !es->ns)
//       continue;
//
//     double mse = es->ns->mse;
//
//     if (std::isfinite(mse) && mse < cutoffScore)
//     {
//       result->add(es->clone());
//     }
//   }
//
//   return result;
// }

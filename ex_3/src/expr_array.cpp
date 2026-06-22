#include <string>
#include <vector>
#include <set>

#include "../include/expr_array.h"

bool addUniqueTree(
  ExprArray* result,
  Node* tree,
  std::set<std::string>& seen) {
  if (!result || !tree)
    return false;

  std::string key = tree->toString();

  if (seen.find(key) != seen.end()) {
    delete tree;
    return false;
  }

  seen.insert(key);

  auto ns = new NodeStats();
  auto es = new ExprStats(tree, ns);

  result->add(es);

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
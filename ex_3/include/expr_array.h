#pragma once

#include <set>
#include <string>
#include <vector>

#include "../include/ast_nodes.h"
#include "../include/ast_nodestats.h"

struct ExprStats {
  Node* n;
  NodeStats* ns;

  ExprStats(Node* node, NodeStats* stats)
    : n(node), ns(stats) {}

  ~ExprStats() {
    delete n;
    delete ns;
  }

  ExprStats* clone() const {
    Node* newNode = n ? n->clone() : nullptr;
    NodeStats* newStats = ns ? new NodeStats(*ns) : new NodeStats();

    return new ExprStats(newNode, newStats);
  }


  ExprStats(const ExprStats&) = delete;
  ExprStats& operator=(const ExprStats&) = delete;
};

class ExprArray {
public:
  std::vector<ExprStats*> items;

  ExprArray(ExprArray&& other) noexcept
    : items(std::move(other.items)) {
    other.items.clear();
  }

  ExprArray& operator=(ExprArray&& other) noexcept {
    if (this != &other) {
      clear();
      items = std::move(other.items);
      other.items.clear();
    }

    return *this;
  }


  ExprArray() = default;

  ~ExprArray() {
    clear();
  }

  int size() const {
    return (int)items.size();
  }

  void clear() {
    for (auto& item : items) {
      delete item;
    }

    items.clear();
  }

  void add(ExprStats* es) {
    items.push_back(es);
  }

  void take(ExprStats* es) {
    items.push_back(es);
  }

  ExprStats* get(int index) {
    if (index < 0 || index >= (int)items.size())
      return nullptr;

    return items[index];
  }

  ExprArray(const ExprArray&) = delete;
  ExprArray& operator=(const ExprArray&);
};

void saveExpressions(
  const std::string& filename,
  const ExprArray& expressions);

ExprArray* loadExpressions(
  const std::string& filename,
  bool loadNodeStats);

bool addUniqueTree(
  ExprArray* result,
  Node* tree,
  std::set<std::string>& seen);

bool addUniqueExprStats(
  ExprArray* result,
  ExprStats* src,
  std::set<std::string>& seen);

ExprArray* removeDuplicateExpressions(const ExprArray* input);

ExprArray* generateBasicExpressionsFromText(
  const std::vector<std::string>& expressionTexts);

ExprArray* generateBasicExpressions();

ExprArray* evolveExpressions(
    ExprArray* input,
    int blockID,
    int genID,
    int stepID,
    int varID,
    const Options& opts,
    float* data);

ExprArray* filterPool(ExprArray* input);

inline double scoreMaxAbsErr(
    int blockID,
    int genID,
    int stepID,
    int varID,
    const Options& opts,
    Node* n,
    float* data);

inline double optimizeCoeffSubset_HillClimbing_Search(
    int blockID,
    int genID,
    int stepID,
    int varID,
    const Options& opts,
    Node* n,
    const std::vector<Node*>& coeffs,
    int lo,
    int hi,
    float* data,
    double initialStep,
    int maxIterations,
    int traceLevel);

double optimize_NodeCoeffs_HillClimbing_Search(
    int blockID,
    int genID,
    int stepID,
    int varID,
    const Options& opts,
    Node* n,
    float* data,
    double initialStep = 1.0,
    int maxIterations = 1000,
    int traceLevel = 0);

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

ExprArray* generateBasicExpressionsFromText(
    const std::vector<std::string>& expressionTexts);

ExprArray* generateBasicExpressions();

ExprArray* evolveExpressions(ExprArray* input);

ExprArray* filterPool(ExprArray* input, double cutoffScore);

double optimize_NodeCoeffs_HillClimbing_Search(
    Node* n,
    int vi,
    float* data,
    int numFloats,
    double initialStep = 1.0,
    double tolerance = 1.0e-6,
    int maxIterations = 1000);
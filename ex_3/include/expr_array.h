#pragma once

#include <string>
#include <vector>

struct ExprStats
{
  Node* n;
  NodeStats* ns;

  ExprStats(Node* node, NodeStats* stats)
    : n(node), ns(stats)
  {
  }

  ~ExprStats()
  {
    delete n;
    delete ns;
  }

  ExprStats* clone() const
  {
    Node* newNode = n ? n->clone() : nullptr;
    NodeStats* newStats = ns ? new NodeStats(*ns) : new NodeStats();

    return new ExprStats(newNode, newStats);
  }

private:
  ExprStats(const ExprStats&);
  ExprStats& operator=(const ExprStats&);
};

class ExprArray {

public:
  std::vector<ExprStats*> items;

  ExprArray(ExprArray&& other) noexcept
    : items(std::move(other.items))
  {
    other.items.clear();
  }

  ExprArray& operator=(ExprArray&& other) noexcept
  {
    if (this != &other)
    {
      clear();
      items = std::move(other.items);
      other.items.clear();
    }

    return *this;
  }


  ExprArray() {

  }

  ~ExprArray() {
    clear();
  }

  int size() const {
    return (int)items.size();
  }

  void clear() {
    for (int i = 0; i < (int)items.size(); i++) {
      delete items[i];
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

private:
  ExprArray(const ExprArray&);
  ExprArray& operator=(const ExprArray&);
};

void saveExpressions(
  const std::string& filename,
  const ExprArray& expressions);

ExprArray loadExpressions(
  const std::string& filename,
  bool loadNodeStats);
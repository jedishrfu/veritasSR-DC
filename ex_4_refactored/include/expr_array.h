#pragma once

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

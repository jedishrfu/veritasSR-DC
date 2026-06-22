#pragma once

#include <string>

enum NodeKind {
  NODE_VALUE,
  NODE_VARIABLE,
  NODE_UNARY,
  NODE_BINARY
};

enum OpKind {
  OP_NONE,

  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,

  OP_SIN,
  OP_COS,
  OP_EXP,
  OP_LOG
};

class ExprArray;

class Node {
private:
  NodeKind kind;
  OpKind op;

  double cv;
  int vi;

  Node* lc;
  Node* rc;

  Node(NodeKind k, OpKind o, double cv, int vi, Node* lc, Node* rc)
    : kind(k), op(o), cv(cv), vi(vi), lc(lc), rc(rc) {}

public:
  ~Node() {
    delete lc;
    delete rc;
  }

  Node* clone() const {
    Node* newLc = lc ? lc->clone() : nullptr;
    Node* newRc = rc ? rc->clone() : nullptr;

    return new Node(
      kind,
      op,
      cv,
      vi,
      newLc,
      newRc
    );
  }

  double getNodeCoeff() const {
    return (kind == NODE_VALUE) ? cv : NAN;
  }

  bool isCoeffNode() const {
    return kind == NODE_VALUE;
  }

  void setNodeCoeff(double ncv) {
    if (kind == NODE_VALUE) cv = ncv;
  }

  static Node* makeCoeffValue(double cv) {
    return new Node(NODE_VALUE, OP_NONE, cv, -1, nullptr, nullptr);
  }

  static Node* makeVariable(int vi) {
    return new Node(NODE_VARIABLE, OP_NONE, 0.0, vi, nullptr, nullptr);
  }

  static Node* makeUnary(OpKind op, Node* lc) {
    return new Node(NODE_UNARY, op, 0.0, -1, lc, nullptr);
  }

  static Node* makeBinary(OpKind op, Node* lc, Node* rc) {
    return new Node(NODE_BINARY, op, 0.0, -1, lc, rc);
  }

  NodeKind getKind() const { return kind; }
  OpKind getOp() const { return op; }
  int getVarIndex() const { return vi; }
  Node* getLeftChild() const { return lc; }
  Node* getRightChild() const { return rc; }

  double eval() const;
  std::string toString() const;
};

int countNodeCoeffs(const Node* node);
void setNodeCoeffs(Node* node, double* ncv);
int countNodes(const Node* node);
int treeDepth(const Node* node);

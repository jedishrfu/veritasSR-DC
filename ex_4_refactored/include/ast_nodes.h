/*
 * VeritasSR
 * Symbolic Regression and Equation Discovery System
 *
 * Copyright (c) 2026 James M. McArdle
 * All Rights Reserved.
 *
 * File: ast_nodes.h
 * Purpose: Project header copied from the current working prototype.
 *
 * SPDX-License-Identifier: Proprietary
 */
#pragma once

#include <stdio.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <string>
#include <sstream>
#include <vector>
#include <random>

enum NodeKind
{
  NODE_VALUE,
  NODE_VARIABLE,
  NODE_UNARY,
  NODE_BINARY
};

enum OpKind
{
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

class Node
{
private:
  NodeKind kind;
  OpKind op;

  double cv;
  int vi;

  Node* lc;
  Node* rc;

  Node(NodeKind k, OpKind o, double cv, int vi, Node* lc, Node* rc)
    : kind(k), op(o), cv(cv), vi(vi), lc(lc), rc(rc)
  {
  }

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

    return;
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

inline int countNodeCoeffs(const Node* node) {
  if (node == nullptr) return 0;

  switch (node->getKind()) {
  case NODE_VALUE:
    return 1;

  case NODE_VARIABLE:
    return 0;

  case NODE_UNARY:
    return countNodeCoeffs(node->getLeftChild());

  case NODE_BINARY:
    return countNodeCoeffs(node->getLeftChild()) +
      countNodeCoeffs(node->getRightChild());
  }

  return 0;
}

inline void setNodeCoeffsRec(Node* node, double* ncv, int& valueNodeCount)
{
  if (node == nullptr)
    return;

  switch (node->getKind())
  {
  case NODE_VALUE:
    node->setNodeCoeff(ncv[valueNodeCount]);
    valueNodeCount++;
    return;

  case NODE_VARIABLE:
    return;

  case NODE_UNARY:
    setNodeCoeffsRec(node->getLeftChild(), ncv, valueNodeCount);
    return;

  case NODE_BINARY:
    setNodeCoeffsRec(node->getLeftChild(), ncv, valueNodeCount);
    setNodeCoeffsRec(node->getRightChild(), ncv, valueNodeCount);
    return;
  }
}

inline void setNodeCoeffs(Node* node, double* ncv)
{
  if (node == nullptr)
    return;

  int coeffCount = countNodeCoeffs(node);

  if (coeffCount == 0)
    return;

  if (ncv == nullptr)
  {
    logError("ERROR in setNodeCoeffs(): double* ncv is nullptr");
    return;
  }

  int valueNodeCount = 0;
  setNodeCoeffsRec(node, ncv, valueNodeCount);
}

inline int countNodes(const Node* node) {
  if (node == nullptr)
    return 0;

  switch (node->getKind()) {
  case NODE_VALUE:
  case NODE_VARIABLE:
    return 1;

  case NODE_UNARY:
    return 1 + countNodes(node->getLeftChild());

  case NODE_BINARY:
    return 1 +
      countNodes(node->getLeftChild()) +
      countNodes(node->getRightChild());
  }

  return 0;
}

inline int treeDepth(const Node* node) {
  if (node == nullptr)
    return 0;

  switch (node->getKind()) {
  case NODE_VALUE:
  case NODE_VARIABLE:
    return 1;

  case NODE_UNARY:
    return 1 + treeDepth(node->getLeftChild());

  case NODE_BINARY: {
    int ld = treeDepth(node->getLeftChild());
    int rd = treeDepth(node->getRightChild());

    return 1 + (ld > rd ? ld : rd);
  }
  }

  return 0;
};

class NodeStats
{
public:
  long count;

  long numWithinTol;
  long numOutsideTol;

  double sumError;
  double sumSquaredError;
  double maxAbsError;
  double meanOriginal;

  double mae;
  double mse;
  double rmse;
  double psnr;

  int nodeCount;
  int depth;

  NodeStats()
    : count(0),
    numWithinTol(0),
    numOutsideTol(0),
    sumError(0.0),
    sumSquaredError(0.0),
    maxAbsError(0.0),
    meanOriginal(0.0),
    mae(0.0),
    mse(0.0),
    rmse(0.0),
    psnr(0.0),
    nodeCount(0),
    depth(0) {
  }

  std::string toString() const
  {
    std::ostringstream ss;

    ss << "count=" << count
      << " nodes=" << nodeCount
      << " depth=" << depth
      << " mae=" << mae
      << " mse=" << mse
      << " rmse=" << rmse
      << " psnr=" << psnr
      << " maxAbs=" << maxAbsError
      << " within=" << numWithinTol
      << " outside=" << numOutsideTol;

    return ss.str();
  }
};


struct VarInfo
{
  std::string name;
  double value;

  VarInfo()
    : name(""), value(0.0)
  {
  }

  VarInfo(const std::string& vname, double vvalue)
    : name(vname), value(vvalue)
  {
  }
};

class VarTable
{
private:
  static const int MAX_VARS = 10;

  VarInfo vars[MAX_VARS];
  int count;

public:
  VarTable()
    : count(1)
  {
    vars[0] = VarInfo("x", 0.0);
  }

  void clear()
  {
    count = 0;
  }

  void resetToDefault()
  {
    count = 1;
    vars[0] = VarInfo("x", 0.0);
  }

  bool addVar(const std::string& name, double value = 0.0)
  {
    if (count >= MAX_VARS)
      return false;

    vars[count++] = VarInfo(name, value);
    return true;
  }

  bool setVarsFromColonList(const std::string& spec)
  {
    clear();

    if (spec.empty())
    {
      resetToDefault();
      return true;
    }

    size_t start = 0;

    while (start < spec.size())
    {
      size_t end = spec.find(':', start);
      std::string name =
        (end == std::string::npos)
        ? spec.substr(start)
        : spec.substr(start, end - start);

      if (!name.empty())
      {
        if (!addVar(name, 0.0))
          return false;
      }

      if (end == std::string::npos)
        break;

      start = end + 1;
    }

    if (count == 0)
      resetToDefault();

    return true;
  }

  int getCount() const
  {
    return count;
  }

  const std::string& getName(int index) const
  {
    static const std::string empty = "";

    if (index < 0 || index >= count)
      return empty;

    return vars[index].name;
  }

  double getValue(int index) const
  {
    if (index < 0 || index >= count)
      return 0.0;

    return vars[index].value;
  }

  void setValue(int index, double value)
  {
    if (index < 0 || index >= count)
      return;

    vars[index].value = value;
  }
};

extern VarTable varTable;
inline void computeScore(
  int vi,
  double tol,
  float* data,
  int numFloats,
  Node* n,
  NodeStats* ns)
{
  if (ns == nullptr)
    return;

  *ns = NodeStats();

  ns->count = numFloats;
  ns->nodeCount = countNodes(n);
  ns->depth = treeDepth(n);

  double peakValue = 0.0;

  for (int i = 0; i < numFloats; i++)
  {
    varTable.setValue(vi, (double)i);
    double computed = n->eval();

    if (computed != computed)
    {
      computed = 1.0e99;
    }

    double error = computed - data[i];
    double absError = fabs(error);

    ns->sumError += absError;
    ns->sumSquaredError += error * error;
    ns->meanOriginal += data[i];

    if (absError > ns->maxAbsError)
    {
      ns->maxAbsError = absError;
    }

    if (absError <= tol)
    {
      ns->numWithinTol++;
    }
    else
    {
      ns->numOutsideTol++;
    }

    if (fabs(data[i]) > peakValue)
    {
      peakValue = fabs(data[i]);
    }
  }

  if (numFloats > 0)
  {
    ns->meanOriginal /= numFloats;
    ns->mae = ns->sumError / numFloats;
    ns->mse = ns->sumSquaredError / numFloats;
    ns->rmse = sqrt(ns->mse);

    if (ns->rmse > 0.0 && peakValue > 0.0)
    {
      ns->psnr = 20.0 * log10(peakValue / ns->rmse);
    }
    else
    {
      ns->psnr = 999.0;
    }
  }
};
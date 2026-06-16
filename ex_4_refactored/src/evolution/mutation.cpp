/*
 * VeritasSR
 * Symbolic Regression and Equation Discovery System
 *
 * Copyright (c) 2026 James M. McArdle
 * All Rights Reserved.
 *
 * File: mutation.inc
 * Purpose: Random constant, operator, and delete-node mutation helpers.
 *
 * Notes:
 *   This file is included into src/main.cpp to preserve the current
 *   single-translation-unit prototype behavior while making the source
 *   easier to navigate. A future pass can convert these .inc modules
 *   into separately compiled .cpp files with explicit headers.
 *
 * SPDX-License-Identifier: Proprietary
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <string>
#include <vector>
#include <set>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <cmath>

#include "ast_nodes.h"
#include "logging_code.h"
#include "infix_parsing_code.h"

OpKind randomUnaryOp()
{
  switch (rand() % 4)
  {
  case 0: return OP_SIN;
  case 1: return OP_COS;
  case 2: return OP_EXP;
  case 3: return OP_LOG;
  }

  return OP_SIN;
}

OpKind randomBinaryOp()
{
  switch (rand() % 4)
  {
  case 0: return OP_ADD;
  case 1: return OP_SUB;
  case 2: return OP_MUL;
  case 3: return OP_DIV;
  }

  return OP_ADD;
}

void collectCoeffNodes(Node* n, std::vector<Node*>& coeffs)
{
  if (!n) return;

  if (n->getKind() == NODE_VALUE)
  {
    coeffs.push_back(n);
    return;
  }

  collectCoeffNodes(n->getLeftChild(), coeffs);
  collectCoeffNodes(n->getRightChild(), coeffs);
}

Node* mutateRandomConstant(const Node* src)
{
  Node* n = src->clone();

  std::vector<Node*> coeffs;
  collectCoeffNodes(n, coeffs);

  if (coeffs.size() > 0)
  {
    int k = rand() % coeffs.size();
    double oldValue = coeffs[k]->getNodeCoeff();
    double delta = randomDouble(-1.0, 1.0);

    coeffs[k]->setNodeCoeff(oldValue + delta);
  }

  return n;
}

int countOpNodes(const Node* n)
{
  if (!n) return 0;

  if (n->getKind() == NODE_UNARY)
    return 1 + countOpNodes(n->getLeftChild());

  if (n->getKind() == NODE_BINARY)
    return 1 + countOpNodes(n->getLeftChild()) + countOpNodes(n->getRightChild());

  return 0;
}

Node* mutateRandomOperatorRec(const Node* src, int target, int& seen)
{
  if (!src) return NULL;

  NodeKind kind = src->getKind();

  if (kind == NODE_VALUE)
    return Node::makeCoeffValue(src->getNodeCoeff());

  if (kind == NODE_VARIABLE)
    return src->clone();

  if (kind == NODE_UNARY)
  {
    OpKind op = src->getOp();

    if (seen == target)
    {
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

  if (kind == NODE_BINARY)
  {
    OpKind op = src->getOp();

    if (seen == target)
    {
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

Node* mutateRandomOperator(const Node* src)
{
  int opCount = countOpNodes(src);

  if (opCount == 0)
    return src->clone();

  int target = rand() % opCount;
  int seen = 0;

  return mutateRandomOperatorRec(src, target, seen);
}

Node* deleteRandomNodeRec(const Node* src, int target, int& seen)
{
  if (!src) return NULL;

  NodeKind kind = src->getKind();

  if (kind == NODE_VALUE)
    return Node::makeCoeffValue(src->getNodeCoeff());

  if (kind == NODE_VARIABLE)
    return src->clone();

  if (kind == NODE_UNARY)
  {
    if (seen == target)
      return src->getLeftChild() ? src->getLeftChild()->clone() : src->clone();

    seen++;

    return Node::makeUnary(
      src->getOp(),
      deleteRandomNodeRec(src->getLeftChild(), target, seen)
    );
  }

  if (kind == NODE_BINARY)
  {
    if (seen == target)
    {
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

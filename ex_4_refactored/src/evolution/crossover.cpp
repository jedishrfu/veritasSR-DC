/*
 * VeritasSR
 * Symbolic Regression and Equation Discovery System
 *
 * Copyright (c) 2026 James M. McArdle
 * All Rights Reserved.
 *
 * File: crossover.inc
 * Purpose: Subtree counting, cloning, replacement, and crossover helpers.
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
#include "evolution/mutation.h"

static int countOpNodes(const Node* n)
{
  if (!n) return 0;

  int count = 0;

  if (n->getKind() == NODE_UNARY || n->getKind() == NODE_BINARY)
    count = 1;

  count += countOpNodes(n->getLeftChild());
  count += countOpNodes(n->getRightChild());

  return count;
}

static Node* deleteRandomNodeRec(
  const Node* n,
  int target,
  int& seen)
{
  if (!n) return nullptr;

  NodeKind kind = n->getKind();

  if (kind == NODE_UNARY || kind == NODE_BINARY)
  {
    if (seen == target)
    {
      if (n->getLeftChild())
        return n->getLeftChild()->clone();

      if (n->getRightChild())
        return n->getRightChild()->clone();

      return Node::makeCoeffValue(0.0);
    }

    seen++;
  }

  switch (kind)
  {
  case NODE_VALUE:
    return Node::makeCoeffValue(n->getNodeCoeff());

  case NODE_VARIABLE:
    return Node::makeVariable(n->getVarIndex());

  case NODE_UNARY:
    return Node::makeUnary(
      n->getOp(),
      deleteRandomNodeRec(
        n->getLeftChild(),
        target,
        seen));

  case NODE_BINARY:
    return Node::makeBinary(
      n->getOp(),
      deleteRandomNodeRec(
        n->getLeftChild(),
        target,
        seen),
      deleteRandomNodeRec(
        n->getRightChild(),
        target,
        seen));
  }

  return nullptr;
}

Node* deleteRandomNode(const Node* src)
{
  if (!src) return nullptr;

  int opCount = countOpNodes(src);

  if (opCount <= 0)
    return src->clone();

  int target = rand() % opCount;
  int seen = 0;

  return deleteRandomNodeRec(src, target, seen);
}

int countAllNodes(const Node* n)
{
  if (!n) return 0;

  return 1
    + countAllNodes(n->getLeftChild())
    + countAllNodes(n->getRightChild());
}

int countVariableNodes(const Node* n)
{
  if (!n) return 0;

  int count = 0;

  if (n->getKind() == NODE_VARIABLE)
    count++;

  count += countVariableNodes(n->getLeftChild());
  count += countVariableNodes(n->getRightChild());

  return count;
}

Node* cloneSubtreeAt(const Node* src, int target, int& seen)
{
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

Node* cloneReplacingSubtreeAt(
  const Node* src,
  int target,
  int& seen,
  const Node* replacement)
{
  if (!src) return NULL;

  if (seen == target)
  {
    seen++;
    return replacement ? replacement->clone() : NULL;
  }

  seen++;

  NodeKind kind = src->getKind();

  if (kind == NODE_VALUE)
    return Node::makeCoeffValue(src->getNodeCoeff());

  if (kind == NODE_VARIABLE)
    return src->clone();

  if (kind == NODE_UNARY)
  {
    return Node::makeUnary(
      src->getOp(),
      cloneReplacingSubtreeAt(src->getLeftChild(), target, seen, replacement)
    );
  }

  if (kind == NODE_BINARY)
  {
    return Node::makeBinary(
      src->getOp(),
      cloneReplacingSubtreeAt(src->getLeftChild(), target, seen, replacement),
      cloneReplacingSubtreeAt(src->getRightChild(), target, seen, replacement)
    );
  }

  return src->clone();
}

Node* crossoverSubtrees(const Node* a, const Node* b)
{
  if (!a || !b) return NULL;

  int countA = countAllNodes(a);
  int countB = countAllNodes(b);

  if (countA == 0 || countB == 0)
    return a->clone();

  int cutA = rand() % countA;
  int cutB = rand() % countB;

  int seenB = 0;
  Node* donor = cloneSubtreeAt(b, cutB, seenB);

  if (!donor)
    return a->clone();

  int seenA = 0;
  Node* child = cloneReplacingSubtreeAt(a, cutA, seenA, donor);

  delete donor;

  return child;
}


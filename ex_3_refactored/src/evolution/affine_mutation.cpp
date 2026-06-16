/*
 * VeritasSR
 * Symbolic Regression and Equation Discovery System
 *
 * Copyright (c) 2026 James M. McArdle
 * All Rights Reserved.
 *
 * File: affine_mutation.inc
 * Purpose: Affine variable mutation helpers.
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

Node* makeAffineVariable(int varIndex)
{
  double a = randomDouble(-10.0, 10.0);
  double b = randomDouble(-10.0, 10.0);

  return Node::makeBinary(
    OP_ADD,
    Node::makeBinary(
      OP_MUL,
      Node::makeCoeffValue(a),
      Node::makeVariable(varIndex)
    ),
    Node::makeCoeffValue(b)
  );
}

Node* mutateRandomVariableToAffineRec(
  const Node* src,
  int target,
  int& seen)
{
  if (!src) return NULL;

  NodeKind kind = src->getKind();

  if (kind == NODE_VALUE)
    return Node::makeCoeffValue(src->getNodeCoeff());

  if (kind == NODE_VARIABLE)
  {
    if (seen == target)
    {
      seen++;
      return makeAffineVariable(src->getVarIndex());
    }

    seen++;
    return src->clone();
  }

  if (kind == NODE_UNARY)
  {
    return Node::makeUnary(
      src->getOp(),
      mutateRandomVariableToAffineRec(src->getLeftChild(), target, seen)
    );
  }

  if (kind == NODE_BINARY)
  {
    return Node::makeBinary(
      src->getOp(),
      mutateRandomVariableToAffineRec(src->getLeftChild(), target, seen),
      mutateRandomVariableToAffineRec(src->getRightChild(), target, seen)
    );
  }

  return src->clone();
}
static int countVariableNodes(const Node* n)
{
  if (!n) return 0;

  int count = 0;

  if (n->getKind() == NODE_VARIABLE)
    count = 1;

  count += countVariableNodes(n->getLeftChild());
  count += countVariableNodes(n->getRightChild());

  return count;
}

Node* mutateRandomVariableToAffine(const Node* src)
{
  int varCount = countVariableNodes(src);

  if (varCount == 0)
    return src ? src->clone() : NULL;

  int target = rand() % varCount;
  int seen = 0;

  return mutateRandomVariableToAffineRec(src, target, seen);
}

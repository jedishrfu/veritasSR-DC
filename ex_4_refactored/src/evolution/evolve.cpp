/*
 * VeritasSR
 * Symbolic Regression and Equation Discovery System
 *
 * Copyright (c) 2026 James M. McArdle
 * All Rights Reserved.
 *
 * File: evolve.inc
 * Purpose: Main evolutionary expansion loop.
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

#include "evolution/mutation.h"
#include "evolution/crossover.h"
#include "core/project_context.h"

int bucket(int n)
{
  if (n <= 150)
    return 1;

  return ((n - 151) / 100) + 2;
}

ExprArray* evolveExpressions(ExprArray* input)
{
  ExprArray* result = new ExprArray();
  std::set<std::string> seen;

  if (!input || input->size() == 0)
    return result;

  enum MutationType
  {
    MUT_CONST = 0,
    MUT_UNARY,
    MUT_BINARY,
    MUT_CHANGE_OP,
    MUT_DELETE_NODE,
    MUT_VARIABLE_AFFINE,
    MUT_CROSSOVER
  };

  const int NUM_CASES = 7;

  int step = bucket(input->size() / 100);

  for (int i = 0; i < input->size(); i += step)
  {
    if (!input->items[i] || !input->items[i]->n)
      continue;

    Node* parent = input->items[i]->n;

    addUniqueExprStats(result, input->items[i], seen);

    Node* newTree = NULL;

    int choice = rand() % NUM_CASES;

    switch (choice)
    {
    case MUT_CONST:
    {
      if (countNodeCoeffs(parent) > 0)
        newTree = mutateRandomConstant(parent);
      else
        newTree = mutateRandomVariableToAffine(parent);

      break;
    }

    case MUT_UNARY:
    {
      newTree = Node::makeUnary(
        randomUnaryOp(),
        parent->clone()
      );

      break;
    }

    case MUT_BINARY:
    {
      int j = rand() % input->size();

      if (j == i && input->size() > 1)
        j = (j + 1) % input->size();

      if (!input->items[j] || !input->items[j]->n)
        break;

      Node* other = input->items[j]->n;
      OpKind op = randomBinaryOp();

      if (rand() % 2 == 0)
      {
        newTree = Node::makeBinary(
          op,
          parent->clone(),
          other->clone()
        );
      }
      else
      {
        newTree = Node::makeBinary(
          op,
          other->clone(),
          parent->clone()
        );
      }

      break;
    }

    case MUT_CHANGE_OP:
    {
      newTree = mutateRandomOperator(parent);
      break;
    }

    case MUT_DELETE_NODE:
    {
      newTree = deleteRandomNode(parent);
      break;
    }

    case MUT_VARIABLE_AFFINE:
    {
      newTree = mutateRandomVariableToAffine(parent);
      break;
    }

    case MUT_CROSSOVER:
    {
      int j = rand() % input->size();

      if (j == i && input->size() > 1)
        j = (j + 1) % input->size();

      if (!input->items[j] || !input->items[j]->n)
        break;

      newTree = crossoverSubtrees(parent, input->items[j]->n);
      break;
    }

    default:
      break;
    }

    if (newTree)
      addUniqueTree(result, newTree, seen);
  }

  return result;
}

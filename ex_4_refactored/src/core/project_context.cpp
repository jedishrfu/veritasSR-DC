/*
 * VeritasSR
 * Symbolic Regression and Equation Discovery System
 *
 * Copyright (c) 2026 James M. McArdle
 * All Rights Reserved.
 *
 * File: project_context.inc
 * Purpose: Project-wide includes, options, default variables, and uniqueness helpers.
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

#include "exprgen.h"
#include "ast_nodes.h"
#include "expr_array.h"

#define FILENAME_SIZE 256
#define DEFAULT_COEFF 1
#define MAX_LOOPS 10

VarTable varTable;

struct Options
{
  std::string inputFile;
  std::string zipFile;
  std::string outputFile;

  double tol;
  int maxFloats;
  int maxGenerations;
};

void ensureDefaultVariables()
{
  if (varTable.getCount() == 0)
  {
    varTable.addVar("x", 0.0);
  }
}

std::vector<std::string> makeParserVariableNames()
{
  std::vector<std::string> vars;

  static const char* defaultNames[] =
  {
    "x", "y", "z", "t", "u", "v", "w"
  };

  int count = varTable.getCount();

  for (int i = 0; i < count; i++)
  {
    if (i < 7)
      vars.push_back(defaultNames[i]);
    else
    {
      char buf[32];
      snprintf(buf, sizeof(buf), "x%d", i);
      vars.push_back(buf);
    }
  }

  return vars;
}

bool addUniqueTree(
  ExprArray* result,
  Node* tree,
  std::set<std::string>& seen)
{
  if (!result || !tree)
    return false;

  std::string key = tree->toString();

  if (seen.find(key) != seen.end())
  {
    delete tree;
    return false;
  }

  seen.insert(key);

  NodeStats* ns = new NodeStats();
  ExprStats* es = new ExprStats(tree, ns);
  result->add(es);

  return true;
}

bool addUniqueExprStats(
  ExprArray* result,
  ExprStats* src,
  std::set<std::string>& seen)
{
  if (!src || !src->n)
    return false;

  return addUniqueTree(result, src->n->clone(), seen);
}

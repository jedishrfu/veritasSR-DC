/*
 * VeritasSR
 * Symbolic Regression and Equation Discovery System
 *
 * Copyright (c) 2026 James M. McArdle
 * All Rights Reserved.
 *
 * File: scoring_and_args.inc
 * Purpose: Coefficient reset, filtering, output header, and command-line parsing.
 *
 * Notes:
 *   This file is included into src/main.cpp to preserve the current
 *   single-translation-unit prototype behavior while making the source
 *   easier to navigate. A future pass can convert these .inc modules
 *   into separately compiled .cpp files with explicit headers.
 *
 * SPDX-License-Identifier: Proprietary
 */

#include "ast_nodes.h"
#include "logging_code.h"
#include "infix_parsing_code.h"

#include "exprgen.h"

void resetNodeCoeffs(Node* node)
{
  if (!node) return;

  if (node->getKind() == NODE_VALUE)
    node->setNodeCoeff(NAN);

  resetNodeCoeffs(node->getLeftChild());
  resetNodeCoeffs(node->getRightChild());
}

ExprArray* filterPool(
  ExprArray* input,
  double cutoffScore)
{
  ExprArray* result = new ExprArray();

  for (int i = 0; i < input->size(); i++)
  {
    ExprStats* es = input->items[i];

    if (!es || !es->ns)
      continue;

    double mse = es->ns->mse;

    if (std::isfinite(mse) && mse < cutoffScore)
    {
      result->add(es->clone());
    }
  }

  return result;
}

void printHeader()
{
  logPrint("# Symbolic Regression with Genetic Programming\n\n");
  logPrint("Date: %s // %s\n\n", __DATE__, __TIME__);
  logPrint("C++ Version: %s\n\n", getenv("GPP_VERSION") ? getenv("GPP_VERSION") : "unknown");

  char cwd[256];

  if (getcwd(cwd, sizeof(cwd)) != NULL)
  {
    char* last = strrchr(cwd, '/');
    logNote("Experiment: %s\n\n", last ? last + 1 : cwd);
  }

  logPrint("Author: James McArdle\n\n");

  logPrint("This program performs symbolic regression using a simple genetic programming approach.\n\n");
  logPrint("It reads input data from a binary file, generates mathematical expressions, evaluates them against the data, and evolves the expressions over multiple generations to find better fits.\n\n");
}

int processArgs(
  int argc,
  char* argv[],
  Options& opts)
{
  int opt;

  while ((opt = getopt(argc, argv, "i:z:o:e:n:g:")) != -1)
  {
    switch (opt)
    {
    case 'i':
      opts.inputFile = optarg;
      break;

    case 'z':
      opts.zipFile = optarg;
      break;

    case 'o':
      opts.outputFile = optarg;
      break;

    case 'e':
      opts.tolerance = atof(optarg);
      break;

    case 'n':
      opts.maxFloats = atoi(optarg);
      break;

    case 'g':
      opts.maxGenerations = atoi(optarg);
      break;

    default:
      logError("Invalid option: -%c", opt);
      return 1;
    }
  }

  return 0;
}

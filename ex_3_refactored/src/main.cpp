/*
 * VeritasSR
 * Symbolic Regression and Equation Discovery System
 *
 * Copyright (c) 2026 James M. McArdle
 * All Rights Reserved.
 *
 * File: main.cpp
 * Purpose: Program entry point for the VeritasSR prototype.
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

#include "core/project_context.h"
#include "core/options.h"
#include "core/app.h"

#include "generation/generate_basic.h"
#include "evolution/evolve.h"
#include "fitness/filter.h"

int main(int argc, char* argv[])
{
  srand((unsigned int)time(NULL));

  printHeader();

  Options opts;

  //  opts.tolerance = 0.025;
  //  opts.maxFloats = 1000;
  //  opts.maxGenerations = 10;

  double cutoffScore = 100;

  if (processArgs(argc, argv, opts))
    return 1;

  ensureDefaultVariables();

  logNote("- Input file:        %s", opts.inputFile);
  logNote("- Segment file:      %s", opts.zipFile);
  logNote("- Output file:       %s", opts.outputFile);
  logNote("- Tolerance:         %g", opts.tolerance);

  logNote("- Cutoff Score:      %g", cutoffScore);
  logNote("- Max Floats:        %d", opts.maxFloats);
  logNote("- Max Floats:        %d", opts.maxFloats);
  logNote("- Max Generations:   %d", opts.maxGenerations);

  FILE* fp = fopen(opts.inputFile, "rb");

  if (fp == 0)
  {
    logError("Unable to open input file: %s", opts.inputFile);
    return 1;
  }

  fseek(fp, 0, SEEK_END);
  long fileSize = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  long numFloats = fileSize / sizeof(float);

  if (numFloats > opts.maxFloats)
    numFloats = opts.maxFloats;

  float* dataIn = new float[numFloats];

  size_t count = fread(dataIn, sizeof(float), numFloats, fp);
  fclose(fp);

  logNote("\n\nRead in %zu floats\n\n", count);

  ExprArray* exprPool = NULL;

  CPUTimer all_timer;
  all_timer.start();

  for (int generation = 0; generation < opts.maxGenerations; generation++)
  {
    printf("### Generation %d of %d (cutoff=%g)\n",
      generation,
      opts.maxGenerations,
      cutoffScore);

    ExprArray* newPool = NULL;

    CPUTimer ga_loop_timer;
    ga_loop_timer.start();

    if (generation == 0)
    {
      exprPool = generateBasicExpressions();
    }
    else
    {
      int guard = 0;

      while (exprPool->size() < 100 && guard < 20)
      {
        newPool = evolveExpressions(exprPool);

        delete exprPool;
        exprPool = newPool;

        guard++;
      }
    }

    logPrint("#### Generated %d expressions", exprPool->items.size());
    logPrint("| Expr # | MSE | Nodes | Depth | Expression |");
    logPrint("|--------|-----|-------|-------|------------|");

    for (int i = 0; i < exprPool->size(); i++)
    {
      ExprStats* es = exprPool->items[i];

      for (int ii = 0; ii < opts.maxLoops; ii++)
      {
        int ncvcount = countNodeCoeffs(es->n);

        if (ncvcount > 0)
        {
          std::vector<double> ncv(ncvcount);

          for (int j = 0; j < ncvcount; j++)
            ncv[j] = randomDouble(-10.0, 10.0);

          setNodeCoeffs(es->n, ncv.data());
        }

        computeScore(
          0,
          opts.tolerance,
          dataIn,
          (int)numFloats,
          es->n,
          es->ns);
      }

      NodeStats* score = es->ns;

      logPrint(
        "| %d | %g | %d | %d | %s |",
        i,
        score->mse,
        score->nodeCount,
        score->depth,
        es->n->toString().c_str());
    }

    ExprArray* filtered = filterPool(exprPool, cutoffScore);

    delete exprPool;
    exprPool = filtered;

    cutoffScore *= 0.5;

    logPrint("#### Kept %d expressions", exprPool->size());
    logPrint("| Expr # | MSE | Nodes | Depth | Expression |");
    logPrint("|--------|-----|-------|-------|------------|");

    if (exprPool->size() == 0)
    {
      logError("\n\nNo expressions survived cutoff.");
      break;
    }

    for (int i = 0; i < exprPool->size(); i++)
    {
      NodeStats* score = exprPool->items[i]->ns;

      computeScore(
        0,
        opts.tolerance,
        dataIn,
        (int)numFloats,
        exprPool->items[i]->n,
        score);

      logPrint(
        "| %d | %g | %d | %d | %s |",
        i,
        score->mse,
        score->nodeCount,
        score->depth,
        exprPool->items[i]->n->toString().c_str());
    }

    logPrint("\n\nGeneration %d: %.6f s\n",
      generation,
      ga_loop_timer.elapsed());
  }

  logPrint("All %d generations: %.6f s\n",
    opts.maxGenerations,
    all_timer.elapsed());

  delete exprPool;
  delete[] dataIn;

  return 0;
}

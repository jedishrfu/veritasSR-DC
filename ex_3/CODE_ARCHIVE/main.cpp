#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../include/ast_nodes.h"
#include "logging_code.h"

#define FILENAME_SIZE 256

ExprArray *generateBasicExpressions(ExprContext &ctx)
{
  ExprArray *result = new ExprArray();
  
  for (int i = 0; i < ctx.getVarCount(); i++)
  {

    // wrap the original with unary ops
    result->add(new UnaryOpNode(OP_SIN, new VariableNode(i)));
    result->add(new UnaryOpNode(OP_COS, new VariableNode(i)));
    result->add(new UnaryOpNode(OP_EXP, new VariableNode(i)));
    result->add(new UnaryOpNode(OP_LN, new VariableNode(i)));
    result->add(new UnaryOpNode(OP_LOG, new VariableNode(i)));

    result->add(new BinaryOpNode(OP_ADD, new VariableNode(i), new ValueNode(0)));
    result->add(new BinaryOpNode(OP_SUB, new VariableNode(i), new ValueNode(0)));
    result->add(new BinaryOpNode(OP_MUL, new VariableNode(i), new ValueNode(0)));
    result->add(new BinaryOpNode(OP_DIV, new VariableNode(i), new ValueNode(0)));
    result->add(new BinaryOpNode(OP_POW, new VariableNode(i), new ValueNode(0)));
  }

  return result;
}

ExprArray *evolveExpressions(ExprArray *input, ExprContext &ctx)
{
  ExprArray *result = new ExprArray();

  // wrap expression in an intrinsic function
  for (int i = 0; i < input->size(); i++)
  {
    Node *child = input->items[i].expr;

    result->add(child->clone()); // Add the original expression

    // wrap the original with unary ops
    result->add(new UnaryOpNode(OP_SIN, child->clone()));
    result->add(new UnaryOpNode(OP_COS, child->clone()));
    result->add(new UnaryOpNode(OP_EXP, child->clone()));
    result->add(new UnaryOpNode(OP_LN, child->clone()));
    result->add(new UnaryOpNode(OP_LOG, child->clone()));
  }

  // combine pairs of expressions with binary ops excluding self-combinations
  for (int i = 0; i < input->size(); i++)
  {

    Node *left = input->items[i].expr;

    for (int j = 0; j < input->size(); j++)
    {
      if (i == j)
        continue;

      Node *right = input->items[j].expr;

      //// need to renumber the ValueNodes in subexpressions to avoid conflicts

      result->add(new BinaryOpNode(OP_ADD, left->clone(), right->clone()));
      result->add(new BinaryOpNode(OP_SUB, left->clone(), right->clone()));
      result->add(new BinaryOpNode(OP_MUL, left->clone(), right->clone()));
      result->add(new BinaryOpNode(OP_DIV, left->clone(), right->clone()));
      result->add(new BinaryOpNode(OP_POW, left->clone(), right->clone()));
    }
  }

  return result;
}

void assignValueNodeIndices(Node *root)
{
  if (!root)
    return;

  std::vector<Node *> queue;
  queue.push_back(root);

  int nextIndex = 0;

  size_t front = 0;

  while (front < queue.size())
  {
    Node *node = queue[front++];

    if (!node)
      continue;

    if (node->kind == NODE_VALUE)
    {
      delete node;
      node = new ValueNode(nextIndex);
      nextIndex++;
    }

    if (node->left)
      queue.push_back(node->left);

    if (node->right)
      queue.push_back(node->right);
  }
}

ExprArray *filterPool(
    ExprArray *input,
    double cutoffScore)
{
  ExprArray *result = new ExprArray();

  for (int i = 0; i < input->size(); i++)
  {

    ScoreStats *score = input->items[i].score;

    if (score && score->mse < cutoffScore)
    {
      result->take(
          input->items[i].expr,
          input->items[i].score);

      input->items[i].expr = 0;
      input->items[i].score = 0;
    }
  }

  return result;
}

struct Options
{
  char inFile[256];
  char segFile[256];
  char outFile[256];

  double tol;
  int maxFloats;
  int maxGenerations;
};

void printHeader()
{
  logPrint("# Symbolic Regression with Genetic Programming\n\n");
  logPrint("Date: %s // %s\n\n", __DATE__, __TIME__);
  logPrint("C++ Version: %s\n\n", getenv("GPP_VERSION") ? getenv("GPP_VERSION") : "unknown");

  char cwd[256];

  if (getcwd(cwd, sizeof(cwd)) != NULL)
  {
    char *last = strrchr(cwd, '/');
    logNote("Experiment: %s\n\n", last ? last + 1 : cwd);
  }
  logPrint("Author: James McArdle\n\n");

  logPrint("This program performs symbolic regression using a simple genetic programming approach.\n\n");
  logPrint("It reads input data from a binary file, generates mathematical expressions, evaluates them against the data, and evolves the expressions over multiple generations to find better fits.\n\n");
}

int processArgs(
    int argc,
    char *argv[],
    Options &opts)
{
  int opt;

  while ((opt = getopt(argc, argv, "i:z:o:e:n:g:")) != -1)
  {
    switch (opt)
    {

    case 'i':
      strncpy(opts.inFile,
              optarg,
              sizeof(opts.inFile) - 1);

      opts.inFile[sizeof(opts.inFile) - 1] = '\0';
      break;

    case 'z':
      strncpy(opts.segFile,
              optarg,
              sizeof(opts.segFile) - 1);

      opts.segFile[sizeof(opts.segFile) - 1] = '\0';
      break;

    case 'o':
      strncpy(opts.outFile,
              optarg,
              sizeof(opts.outFile) - 1);

      opts.outFile[sizeof(opts.outFile) - 1] = '\0';
      break;

    case 'e':
      opts.tol = atof(optarg);
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

#define MAX_LOOPS 10

int main(int argc, char *argv[])
{
  printHeader();

  Options opts;

  strcpy(opts.inFile, "");
  strcpy(opts.segFile, "");
  strcpy(opts.outFile, "");

  opts.tol = 0.025;
  opts.maxFloats = 1000;
  opts.maxGenerations = 10;
  double cutoffScore = 1.0;

  if (processArgs(argc, argv, opts))
    return 1;

  logNote("- Input file:        %s", opts.inFile);
  logNote("- Segment file:      %s", opts.segFile);
  logNote("- Output file:       %s", opts.outFile);
  logNote("- Tolerance:         %g", opts.tol);

  logNote("- Cutoff Score:      %g", cutoffScore);
  logNote("- Max Floats:        %d", opts.maxFloats);
  logNote("- Max Generations:   %d", opts.maxGenerations);

  FILE *fp = fopen(opts.inFile, "rb");

  if (fp == 0)
  {
    logError("Unable to open input file: %s", opts.inFile);
    return 1;
  }

  fseek(fp, 0, SEEK_END);
  long fileSize = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  long numFloats = fileSize / sizeof(float);

  if (numFloats > opts.maxFloats)
  {
    numFloats = opts.maxFloats;
  }

  float *dataIn = new float[numFloats];

  size_t count = fread(dataIn, sizeof(float), numFloats, fp);
  fclose(fp);

  logNote("Read in %zu floats\n\n", count);

  // -------------------------------------------------------
  //
  // The Genetic Programming loop
  //
  // -------------------------------------------------------

  ExprContext ctx;
  ExprArray *exprPool = 0;

  // timed code section
  // CPUTimer all_timer;
  // all_timer.start();

  for (int generation = 0; generation < opts.maxGenerations; generation++)
  {
    printf("### Generation %d of %d (cutoff=%g)\n", generation, opts.maxGenerations, cutoffScore);

    ExprArray *generated = 0;

    //CPUTimer loop_timer;
    //loop_timer.start();

    if (generation == 0)
    {
      generated = generateBasicExpressions(ctx);
    }
    else
    {
      generated = evolveExpressions(exprPool, ctx);
    }

    delete exprPool;
    exprPool = generated;

    //ScoreStats* score;

    logPrint("#### Generated %d expressions", exprPool->items.size());
    logPrint("| Expr # | MSE | Nodes | Depth | Expression |");
    logPrint("|--------|-----|-------|-------|------------|");

    for (int i = 0; i < exprPool->size(); i++)
    {
      ExprEntry entry = exprPool->items[i];

      for (int ii = 0; ii < MAX_LOOPS; ii++)
      {
        // randomize the expression coefficients
        for (int j = 0; j < countValueNodes(entry.expr); j++)
        {
          entry.setValRegister(
              j,
              (double)rand() / RAND_MAX * 10.0);
          printf("valRegister[%d] = %g\n\n", j, entry.getValRegister(j));
        }

        ScoreStats* score = computeScore(
            opts.tol,
            dataIn,
            (int)numFloats,
            exprPool->items[i].expr,
            ctx, entry);

        if (!exprPool->items[i].score) {
          exprPool->items[i].score=score;
        }

        ScoreStats *oldScore = exprPool->items[i].score;

        if (score->mse < oldScore->mse)
        {
          delete oldScore;
          exprPool->items[i].score = score;
        }
        else
        {
          delete score;
          score = oldScore;
        }
      }

      char exprText[1024];
      exprText[0] = '\0';

      exprPool->get(i)->toString(
          exprText,
          sizeof(exprText),
          ctx, entry);

      ScoreStats *score = exprPool->items[i].score;

      logPrint(
          "| %d | %g | %d | %d | %s |",
          i,
          score->mse,
          score->nodeCount,
          score->depth,
          exprText);
    }

    cutoffScore *= 0.5;

    ExprArray *filtered = filterPool(exprPool, cutoffScore);

    delete exprPool;
    exprPool = filtered;

    logPrint("#### Kept %d expressions", exprPool->size());
    logPrint("| Expr # | MSE | Nodes | Depth | Expression |");
    logPrint("|--------|-----|-------|-------|------------|");

    if (exprPool->size() == 0)
    {
      logError("No expressions survived cutoff.");
      break;
    }

    for (int i = 0; i < exprPool->size(); i++)
    {
      ExprEntry entry = exprPool->items[i];

      ScoreStats *score = computeScore(
          opts.tol,
          dataIn,
          (int)numFloats,
          exprPool->get(i),
          ctx, entry);

      delete exprPool->items[i].score;
      exprPool->items[i].score = score;

      char exprText[1024];
      exprText[0] = '\0';

      exprPool->get(i)->toString(
          exprText,
          sizeof(exprText),
          ctx, entry);

      logPrint(
          "| %d | %g | %d | %d | %s |",
          i,
          score->mse,
          score->nodeCount,
          score->depth,
          exprText);
    }
    //logPrint("Generation %d: %.6f s\n", generation, loop_timer.elapsed());
  }

  //logPrint("All %d generations: %.6f s\n", opts.maxGenerations, all_timer.elapsed());

  delete exprPool;
  delete[] dataIn;

  return 0;
}

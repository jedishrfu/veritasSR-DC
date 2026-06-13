#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <iostream>

#include "ast_nodes.h"
#include "logging_code.h"

#define FILENAME_SIZE 256

VarTable varTable;

ExprArray* generateBasicExpressions()
{
  ExprArray* result = new ExprArray();

  logPrint("VarCount = %d", varTable.getCount());

  for (int i = 0; i < varTable.getCount(); i++)
  {
    Node* un = Node::makeUnary(OP_SIN, Node::makeVariable(i));
    NodeStats* uns = new NodeStats();
    ExprStats* es = new ExprStats(un, uns);
    result->add(es);

    un = Node::makeUnary(OP_COS, Node::makeVariable(i));
    uns = new NodeStats();
    es = new ExprStats(un, uns);
    result->add(es);

    un = Node::makeUnary(OP_EXP, Node::makeVariable(i));
    uns = new NodeStats();
    es = new ExprStats(un, uns);
    result->add(es);

    un = Node::makeUnary(OP_LOG, Node::makeVariable(i));
    uns = new NodeStats();
    es = new ExprStats(un, uns);
    result->add(es);

    Node* bn = Node::makeBinary(OP_ADD, Node::makeVariable(i), Node::makeCoeffValue(0));
    NodeStats* bns = new NodeStats();
    es = new ExprStats(bn, bns);
    result->add(es);

    bn = Node::makeBinary(OP_SUB, Node::makeVariable(i), Node::makeCoeffValue(0));
    bns = new NodeStats();
    es = new ExprStats(bn, bns);
    result->add(es);

    bn = Node::makeBinary(OP_MUL, Node::makeVariable(i), Node::makeCoeffValue(0));
    bns = new NodeStats();
    es = new ExprStats(bn, bns);
    result->add(es);

    bn = Node::makeBinary(OP_DIV, Node::makeVariable(i), Node::makeCoeffValue(0));
    bns = new NodeStats();
    es = new ExprStats(bn, bns);
    result->add(es);
  }

  return result;
}

static OpKind randomUnaryOp()
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

static OpKind randomBinaryOp()
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

static void collectCoeffNodes(Node* n, std::vector<Node*>& coeffs)
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

static Node* mutateRandomConstant(const Node* src)
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

static Node* mutateRandomOperatorRec(const Node* src, int target, int& seen)
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
      op = randomUnaryOp();

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
      op = randomBinaryOp();

    seen++;

    return Node::makeBinary(
      op,
      mutateRandomOperatorRec(src->getLeftChild(), target, seen),
      mutateRandomOperatorRec(src->getRightChild(), target, seen)
    );
  }

  return src->clone();
}

static int countOpNodes(const Node* n)
{
  if (!n) return 0;

  if (n->getKind() == NODE_UNARY)
    return 1 + countOpNodes(n->getLeftChild());

  if (n->getKind() == NODE_BINARY)
    return 1 + countOpNodes(n->getLeftChild()) + countOpNodes(n->getRightChild());

  return 0;
}

static Node* mutateRandomOperator(const Node* src)
{
  int opCount = countOpNodes(src);

  if (opCount == 0)
    return src->clone();

  int target = rand() % opCount;
  int seen = 0;

  return mutateRandomOperatorRec(src, target, seen);
}

static Node* deleteRandomNodeRec(const Node* src, int target, int& seen)
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

static Node* deleteRandomNode(const Node* src)
{
  int opCount = countOpNodes(src);

  if (opCount == 0)
    return src->clone();

  int target = rand() % opCount;
  int seen = 0;

  return deleteRandomNodeRec(src, target, seen);
}

int bucket(int n)
{
  if (n <= 150)
    return 1;

  return ((n - 151) / 100) + 2;
}

ExprArray* evolveExpressions(ExprArray* input)
{
  ExprArray* result = new ExprArray();

  if (!input || input->size() == 0)
    return result;

  enum MutationType
  {
    MUT_CONST_0 = 0,
    MUT_CONST_1,
    MUT_CONST_2,
    MUT_CONST_3,
    MUT_UNARY,
    MUT_BINARY,
    MUT_CHANGE_OP,
    MUT_DELETE_NODE,
    MUT_COPY
  };

  const int NUM_CASES = 9;

  int step = bucket(input->size() / 100);
  for (int i = 0; i < input->size(); i += step)
  {
    if (!input->items[i] || !input->items[i]->n)
      continue;

    result->add(input->items[i]->clone());

    Node* parent = input->items[i]->n;
    Node* newTree = NULL;

    int choice = rand() % NUM_CASES;

    switch (choice)
    {
    case MUT_CONST_0:
    case MUT_CONST_1:
    case MUT_CONST_2:
    case MUT_CONST_3:
    {
      newTree = mutateRandomConstant(parent);
      break;
    }

    case MUT_UNARY:
    {
      newTree = Node::makeUnary(randomUnaryOp(), parent->clone());
      break;
    }

    case MUT_BINARY:
    {
      int j = rand() % input->size();

      if (!input->items[j] || !input->items[j]->n)
        break;

      Node* other = input->items[j]->n;
      OpKind op = randomBinaryOp();

      if (rand() % 2 == 0)
        newTree = Node::makeBinary(op, parent->clone(), other->clone());
      else
        newTree = Node::makeBinary(op, other->clone(), parent->clone());

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

    case MUT_COPY:
    default:
    {
      newTree = parent->clone();
      break;
    }
    }

    if (newTree)
    {
      NodeStats* ns = new NodeStats();
      ExprStats* es = new ExprStats(newTree, ns);
      result->add(es);
    }
  }

  return result;
}

#define DEFAULT_COEFF 1

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

struct Options
{
  std::string input_file;
  std::string segments_file;
  std::string decom_file;

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
      opts.input_file = optarg;

      break;

    case 'z':
      opts.segments_file = optarg;
      break;

    case 'o':
      opts.decom_file = optarg;
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

int main(int argc, char* argv[])
{
  printHeader();

  Options opts;

  opts.tol = 0.025;
  opts.maxFloats = 1000;
  opts.maxGenerations = 10;

  double cutoffScore = 100;

  if (processArgs(argc, argv, opts))
    return 1;

  logNote("- Input file:        %s", opts.input_file.c_str());
  logNote("- Segment file:      %s", opts.segments_file.c_str());
  logNote("- Output file:       %s", opts.decom_file.c_str());
  logNote("- Tolerance:         %g", opts.tol);

  logNote("- Cutoff Score:      %g", cutoffScore);
  logNote("- Max Floats:        %d", opts.maxFloats);
  logNote("- Max Generations:   %d", opts.maxGenerations);

  FILE* fp = fopen(opts.input_file.c_str(), "rb");

  if (fp == 0)
  {
    logError("Unable to open input file: %s", opts.input_file.c_str());
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

  float* dataIn = new float[numFloats];

  size_t count = fread(dataIn, sizeof(float), numFloats, fp);
  fclose(fp);

  logNote("\n\nRead in %zu floats\n\n", count);

  // -------------------------------------------------------
  //
  // The Genetic Programming loop
  //
  // -------------------------------------------------------

  ExprArray* exprPool = nullptr;

  // timed code section
  CPUTimer all_timer;
  all_timer.start();

  for (int generation = 0; generation < opts.maxGenerations; generation++)
  {
    printf("### Generation %d of %d (cutoff=%g)\n", generation, opts.maxGenerations, cutoffScore);

    ExprArray* newPool = 0;

    CPUTimer ga_loop_timer;
    ga_loop_timer.start();

    if (generation == 0) {
      exprPool = generateBasicExpressions();
    }
    else {
      while (exprPool->size() < 100) {
        newPool = evolveExpressions(exprPool);
        delete exprPool;
        exprPool = newPool;
      }
    }


    logPrint("#### Generated %d expressions", exprPool->items.size());
    logPrint("| Expr # | MSE | Nodes | Depth | Expression |");
    logPrint("|--------|-----|-------|-------|------------|");

    for (int i = 0; i < exprPool->size(); i++)
    {
      ExprStats* es = exprPool->items[i];

      for (int ii = 0; ii < MAX_LOOPS; ii++)
      {
        // randomize the expression coefficients
        int ncvcount = countNodeCoeffs(es->n);

        if (ncvcount > 0)
        {
          std::vector<double> ncv(ncvcount);

          for (int j = 0; j < ncvcount; j++)
          {
            ncv[j] = randomDouble(-10.0, 10.0);
          }

          setNodeCoeffs(es->n, ncv.data());
        }


        computeScore(
          0,
          opts.tol,
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
      logError("No expressions survived cutoff.");
      break;
    }

    for (int i = 0; i < exprPool->size(); i++)
    {
      NodeStats* score = exprPool->items[i]->ns;
      computeScore(
        0,
        opts.tol,
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
    logPrint("Generation %d: %.6f s\n", generation, ga_loop_timer.elapsed());
  }

  logPrint("All %d generations: %.6f s\n", opts.maxGenerations, all_timer.elapsed());

  delete exprPool;
  delete[] dataIn;

  return 0;
}

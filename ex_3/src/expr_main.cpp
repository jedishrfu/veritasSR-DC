#include <cstdio>
#include <cstdlib>
#include <sys/time.h>
#include <unistd.h>

#include <string>
#include <set>

#include "../include/ast_nodes.h"
#include "../include/ast_nodestats.h"
#include "../include/expr_array.h"
#include "../include/util_code.h"
#include "../include/var_table.h"
#include "../include/expr_parser.h"
#include "../include/expr_simplifier.h"

#define FILENAME_SIZE 256
#define DEFAULT_COEFF 1
#define MAX_LOOPS 10

VarTable varTable;

struct CPUTimer {
  timeval beg, end;

  CPUTimer() {}

  ~CPUTimer() {}

  void start() { gettimeofday(&beg, nullptr); }

  double elapsed() {
    gettimeofday(&end, nullptr);
    return end.tv_sec - beg.tv_sec + (end.tv_usec - beg.tv_usec) / 1000000.0;
  }
};


void printHeader() {
  logPrint("# Symbolic Regression with Genetic Programming\n\n");
  logPrint("Date: %s // %s\n\n", __DATE__, __TIME__);
  logPrint("C++ Version: %s\n\n", getenv("GPP_VERSION") ? getenv("GPP_VERSION") : "unknown");

  char cwd[256];

  if (getcwd(cwd, sizeof(cwd)) != nullptr) {
    char* last = strrchr(cwd, '/');
    logNote("Experiment: %s\n\n", last ? last + 1 : cwd);
  }

  logPrint("Author: James McArdle\n\n");

  logPrint("This program performs symbolic regression using a simple genetic programming approach.\n\n");
  logPrint(
    "It reads input data from a binary file, generates mathematical expressions, evaluates them against the data, and evolves the expressions over multiple generations to find better fits.\n\n");
}

struct Options {
  std::string input_file;
  std::string segments_file;
  std::string decom_file;

  double tol;
  int maxFloats;
  int maxGenerations;
};

int processArgs(
  int argc,
  char* argv[],
  Options& opts) {
  int opt;

  while ((opt = getopt(argc, argv, "i:z:o:e:n:g:")) != -1) {
    switch (opt) {
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

int main(int argc, char* argv[]) {
  srand((unsigned int)time(NULL));

  printHeader();

  Options opts;

  opts.tol = 0.025;
  opts.maxFloats = 1000;
  opts.maxGenerations = 10;

  if (processArgs(argc, argv, opts))
    return 1;

  ensureDefaultVariables();

  double cutoffScore = 1024 * opts.tol;

  logNote("- Input file:        %s", opts.input_file.c_str());
  logNote("- Segment file:      %s", opts.segments_file.c_str());
  logNote("- Output file:       %s", opts.decom_file.c_str());
  logNote("- Tolerance:         %g", opts.tol);

  logNote("- Cutoff Score:      %g", cutoffScore);
  logNote("- Max Floats:        %d", opts.maxFloats);
  logNote("- Max Generations:   %d", opts.maxGenerations);

  FILE* fp = fopen(opts.input_file.c_str(), "rb");

  if (fp == 0) {
    logError("Unable to open input file: %s", opts.input_file.c_str());
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

  for (int generation = 0; generation < opts.maxGenerations; generation++) {
    printf("### Generation %d of %d (cutoff=%g)\n",
           generation,
           opts.maxGenerations,
           cutoffScore);

    ExprArray* newPool = NULL;

    CPUTimer ga_loop_timer;
    ga_loop_timer.start();

    if (generation == 0) {
      // if (!true) {
      //   Node* my_expr = parseExpression("ax + b", makeParserVariableNames());
      //   if (getLastParseError() != "") {
      //     ExprStats* my_expr_stats = new ExprStats(my_expr,nullptr);
      //     exprPool->add(my_expr_stats);
      //   }
      // }
      // else {
      std::vector<std::string> basicExpressions = {
        "x",
        "y",
        "z",

        "x+1",
        "x-1",
        "x*2",
        "x/2",

        "x+y",
        "x-y",
        "x*y",
        "x/y",

        "sin(x)",
        "cos(x)",
        "tan(x)",
        "exp(x)",
        "log(x)",
        "sqrt(abs(x))",

        "x*x",
        "x*x*x",
        "(x+y)/2",
        "(x-y)*(x+y)"
      };
      exprPool = generateBasicExpressionsFromText(basicExpressions);
    }
    else {
      int guard = 0;

      while (exprPool->size() < 100 && guard < 20) {
        newPool = evolveExpressions(exprPool);

        delete exprPool;
        ExprSimplifier::simplifyExpressionArray(*newPool, false);
        exprPool = newPool;
        guard++;
      }
    }
    if (generation == 9) {
      saveExpressions("gen_x.ast", *exprPool);
      ExprArray* testReload = loadExpressions("gen_x.ast", false);

      for (int i = 0; i < exprPool->size(); i++) {
        std::string s1 = exprPool->items[i]->n->toString();
        std::string s2 = testReload->items[i]->n->toString();

        if (s1 != s2) {
          std::cout << "\n- Expected: " << s1 << '\n';
          std::cout << "\n- Actual:   " << s2 << '\n';
        }
      }
    }

    logPrint("#### Generated %d expressions", exprPool->items.size());
    logPrint("| Expr # | MSE | Nodes | Depth | Expression |");
    logPrint("|--------|-----|-------|-------|------------|");

    for (int i = 0; i < exprPool->size(); i++) {
      ExprStats* es = exprPool->items[i];
      Node* n = es->n;

      double vi = varTable.getValue(0);

      int numCoeffs = countNodeCoeffs(exprPool->items[i]->n);

      optimize_NodeCoeffs_HillClimbing_Search(
        n,
        vi,
        dataIn,
        numCoeffs,
        1.0,
        opts.tol,
        1000
      );

      NodeStats::computeScore(
        0,
        opts.tol,
        dataIn,
        (int)numFloats,
        es->n,
        es->ns);

      NodeStats* score = es->ns;

      logPrint(
        "| %d | %g | %d | %d | %s |",
        i,
        score->mse,
        score->nodeCount,
        score->depth,
        es->n->toString().c_str());
    }

    NodeStats avg = averageNodeStats(*exprPool);

    logPrint("\n- average MSE: %.1f", avg.mse);

    ExprArray* filtered = filterPool(exprPool, cutoffScore);

    delete exprPool;
    exprPool = filtered;

    cutoffScore *= 0.5;

    logNote("\n- Cutoff Score:      %g", cutoffScore);

    logPrint("#### Kept %d expressions", exprPool->size());
    logPrint("| Expr # | MSE | Nodes | Depth | Expression |");
    logPrint("|--------|-----|-------|-------|------------|");

    if (exprPool->size() == 0) {
      logError("\n\nNo expressions survived cutoff.");
      break;
    }

    for (int i = 0; i < exprPool->size(); i++) {
      NodeStats* score = exprPool->items[i]->ns;

      NodeStats::computeScore(
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

    logPrint("\n\nGeneration %d: %.6f s\n",
             generation,
             ga_loop_timer.elapsed());
  }

  logPrint(
    "All %d generations: %.6f s\n"
    ,
    opts
    .
    maxGenerations
    ,
    all_timer
    .
    elapsed()
  );

  delete
    exprPool;
  delete
    [] dataIn;

  return
    0;
}

#include <cstdio>
#include <sys/time.h>
#include <unistd.h>

#include <string>
#include <set>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "../include/ast_binary_io.h"
#include "../include/block_characterizer.h"
#include "../include/ast_nodes.h"
#include "../include/ast_nodestats.h"
#include "../include/expr_array.h"
#include "../include/util_code.h"
#include "../include/var_table.h"
#include "../include/expr_simplifier.h"

#define FILENAME_SIZE 256
#define DEFAULT_COEFF 1
#define MAX_LOOPS 10

thread_local VarTable varTable;

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

  logPrint(
    "This program uses symbolic regression as a form of lossy numerical data "
    "compression. It reads a binary file containing 32-bit floating-point values "
    "and divides the input data into independently processed blocks, typically "
    "containing between 250 and 2,000 values.\n\n"
  );

  logPrint(
    "For each block, a genetic programming algorithm generates, evaluates, and "
    "evolves candidate mathematical expressions. The program selects the most "
    "compact expression capable of reproducing the block's data within a "
    "user-defined error tolerance. Each selected expression becomes one segment "
    "of a piecewise mathematical representation of the original dataset.\n\n"
  );

  logPrint(
    "The resulting compressed file stores the best expression for each block "
    "instead of storing every original floating-point value. During "
    "decompression, these expressions are evaluated across their corresponding "
    "block ranges to reconstruct the complete dataset. The reconstructed values "
    "approximate the original data while remaining within the specified "
    "tolerance.\n\n"
  );

  logPrint(
    "This approach transforms a sequence of floating-point samples into a compact "
    "piecewise symbolic model, balancing reconstruction accuracy against "
    "expression complexity and compressed file size.\n\n"
  );
}

int processArgs(
  int argc,
  char* argv[],
  Options& opts) {
  int opt;

  while ((opt = getopt(argc, argv, "i:z:o:e:n:g:s:b:c:w:")) != -1) {
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

    case 's':
      opts.saveInterval = atoi(optarg);
      break;

    case 'b':
      opts.blockSize = atoi(optarg);
      break;

    case 'c':
      opts.evolutionRates.crossoverProbability = atof(optarg);
      break;

    case 'w': {
      const std::string setting(optarg);
      const std::size_t separator = setting.find('=');
      if (separator == std::string::npos ||
          !opts.evolutionRates.setWeight(
              setting.substr(0, separator),
              atof(setting.substr(separator + 1).c_str()))) {
        logError("Invalid mutation weight: %s", optarg);
        return 1;
      }
      break;
    }

    default:
      logError("Invalid option: -%c", opt);
      return 1;
    }
  }

  if (opts.maxGenerations <= 0) {
    logError("Number of generations (-g) must be greater than zero");
    return 1;
  }

  if (opts.saveInterval <= 0) {
    logError("Generation save interval (-s) must be greater than zero");
    return 1;
  }

  if (opts.blockSize <= 0) {
    logError("Block size (-b) must be greater than zero");
    return 1;
  }

  if (opts.evolutionRates.crossoverProbability < 0.0 ||
      opts.evolutionRates.crossoverProbability > 1.0) {
    logError("Crossover probability (-c) must be between zero and one");
    return 1;
  }

  if (opts.evolutionRates.totalMutationWeight() <= 0.0 &&
      opts.evolutionRates.crossoverProbability < 1.0) {
    logError("At least one mutation weight must be greater than zero");
    return 1;
  }

  return 0;
}

bool printBlockExprArrayStats(
    int varID,
    const Options& opts,
    int numFloats,
    float* dataIn,
    ExprArray* exprPool)
{
  logPrint("## Block expressions %d", exprPool->size());
  logPrint("| Block # | MaxAE | Nodes | Depth | Expression |");
  logPrint("|---------|-------|-------|-------|------------|");

  for (int i = 0; i < exprPool->size(); ++i) {
    auto* item = exprPool->items[i];

    if (item == nullptr || item->n == nullptr || item->ns == nullptr) {
      logError("Invalid expression at block %d", i);
      continue;
    }

    Options blockOpts = opts;
    blockOpts.blockOffset = static_cast<long>(i) * opts.blockSize;
    blockOpts.blockSize = std::min(
        opts.blockSize,
        numFloats - static_cast<int>(blockOpts.blockOffset));

    NodeStats::computeScore(
        i,          // Each expression belongs to block i
        999,
        999,
        varID,
        blockOpts,
        dataIn,
        item->n,
        item->ns);

    logPrint(
        "| %d | %g | %d | %d | %s |",
        i,
        item->ns->maxAbsError,
        item->ns->nodeCount,
        item->ns->depth,
        item->n->canonicalString().c_str());
  }

  return false;
}

bool reconstructAndAnalyze(
    int varID,
    const Options& opts,
    const float* dataIn,
    int numFloats,
    const ExprArray* blockExpressions)
{
  if (dataIn == nullptr || blockExpressions == nullptr || opts.blockSize <= 0) {
    logError("Cannot reconstruct data: invalid input");
    return false;
  }

  const int expectedBlocks =
      numFloats == 0 ? 0 : 1 + (numFloats - 1) / opts.blockSize;
  if (blockExpressions->size() != expectedBlocks) {
    logError(
        "Cannot reconstruct data: %d samples with block size %d require %d "
        "blocks, but %d expressions were loaded",
        numFloats,
        opts.blockSize,
        expectedBlocks,
        blockExpressions->size());
    return false;
  }

  auto* dataOut = new float[numFloats];

  double sumAbsError = 0.0;
  double sumSquaredError = 0.0;
  double maxAbsError = 0.0;
  double peakValue = 0.0;
  int maxErrorIndex = -1;
  int numWithinTol = 0;

  for (int blockID = 0; blockID < blockExpressions->size(); ++blockID) {
    const ExprStats* expression = blockExpressions->items[blockID];
    if (expression == nullptr || expression->n == nullptr) {
      logError("Cannot reconstruct data: invalid expression at block %d", blockID);
      delete[] dataOut;
      return false;
    }

    const int blockOffset = blockID * opts.blockSize;
    const int blockLength =
        std::min(opts.blockSize, numFloats - blockOffset);

    for (int i = 0; i < blockLength; ++i) {
      const int index = blockOffset + i;

      // Expressions are fitted using a block-local independent variable.
      varTable.setValue(varID, static_cast<double>(i));
      dataOut[index] = static_cast<float>(expression->n->eval());

      const double error =
          static_cast<double>(dataOut[index]) - static_cast<double>(dataIn[index]);
      const double absError = std::fabs(error);

      sumAbsError += absError;
      sumSquaredError += error * error;
      peakValue = std::max(peakValue, std::fabs(static_cast<double>(dataIn[index])));

      if (absError > maxAbsError) {
        maxAbsError = absError;
        maxErrorIndex = index;
      }
      if (absError <= opts.tol) {
        ++numWithinTol;
      }
    }
  }

  const double mae = sumAbsError / numFloats;
  const double mse = sumSquaredError / numFloats;
  const double rmse = std::sqrt(mse);
  const double psnr =
      (rmse > 0.0 && peakValue > 0.0)
          ? 20.0 * std::log10(peakValue / rmse)
          : 999.0;

  logPrint("## Reconstructed data analysis");
  logPrint("| Samples | MaxAE | MAE | MSE | RMSE | PSNR | Within tolerance |");
  logPrint("|---------|-------|-----|-----|------|------|------------------|");
  logPrint(
      "| %d | %.9g | %.9g | %.9g | %.9g | %.9g dB | %d (%.2f%%) |",
      numFloats,
      maxAbsError,
      mae,
      mse,
      rmse,
      psnr,
      numWithinTol,
      100.0 * static_cast<double>(numWithinTol) / numFloats);
  logPrint("\n- Maximum error occurs at sample %d", maxErrorIndex);

  if (!opts.decom_file.empty()) {
    std::ofstream out(
        opts.decom_file,
        std::ios::binary | std::ios::trunc);

    if (!out) {
      logError(
          "Cannot open reconstructed output file: %s",
          opts.decom_file.c_str());
      delete[] dataOut;
      return false;
    }

    out.write(
        reinterpret_cast<const char*>(dataOut),
        static_cast<std::streamsize>(
            static_cast<size_t>(numFloats) * sizeof(float)));
    out.flush();

    if (!out) {
      logError(
          "Failed writing reconstructed output file: %s",
          opts.decom_file.c_str());
      delete[] dataOut;
      return false;
    }

    logPrint(
        "- Wrote %d reconstructed floats to %s",
        numFloats,
        opts.decom_file.c_str());
  }

  delete[] dataOut;
  return true;
}

bool printExprArrayStats(int blockID, int genID, int stepID, int varID, const Options& opts, float* dataIn,
                         ExprArray* exprPool) {
  logPrint("#### Kept %d expressions", exprPool->size());
  logPrint("| Expr # | MaxAE | Nodes | Depth | NaN | Inf | Expression |");
  logPrint("|--------|-------|-------|-------|-----|-----|------------|");

  if (exprPool->size() == 0) {
    logError("\n\nNo expressions survived cutoff.");
    return true;
  }

  for (int i = 0; i < exprPool->size(); i++) {
    NodeStats* score = exprPool->items[i]->ns;

    NodeStats::computeScore(
      blockID,
      genID,
      stepID,
      varID,
      opts,
      dataIn, exprPool->items[i]->n, score);

    logPrint(
      "| %d | %g | %d | %d | %ld | %ld | %s |",
      i,
      score->maxAbsError,
      score->nodeCount,
      score->depth,
      score->numNaN,
      score->numInfinity,
      exprPool->items[i]->n->canonicalString().c_str());
  }
  return false;
}


bool writeCsvExprArrayStats(
  int blockID,
  int genID,
  int stepID,
  int varID,
  const Options& opts,
  float* dataIn,
  ExprArray* exprPool
) {
  auto now = std::chrono::system_clock::now();
  std::time_t nowTime = std::chrono::system_clock::to_time_t(now);

  std::tm tmNow{};
  localtime_r(&nowTime, &tmNow);

  char timestamp[32];
  std::strftime(timestamp, sizeof(timestamp), "%y%m%d_%H%M%S", &tmNow);

  std::ostringstream filename;
  filename << "testdata/" << "expr_array_"
    << blockID << "_"
    << genID << "_"
    << stepID
    << ".csv";

  std::ofstream out(filename.str());

  if (!out.is_open()) {
    return false;
  }

  out << "#"
    << "|MaxAbsoluteError"
    << "|#nodes"
    << "|Depth"
    << "|MAE"
    << "|MSE"
    << "|RMSE"
    << "|PSNR"
    << "|WithinTolerance"
    << "|OutsideTolerance"
    << "|NaNCount"
    << "|InfinityCount"
    << "|BlockSize"
    << "|Expression"
    << "\n";

  out << std::fixed << std::setprecision(3);

  if (exprPool->size() == 0) {
    out << "0"
      << "|0.000"
      << "|0"
      << "|0"
      << "|0.000"
      << "|0.000"
      << "|0.000"
      << "|0.000"
      << "|0"
      << "|0"
      << "|0"
      << "|0"
      << "|" << opts.blockSize
      << "|No expressions survived cutoff."
      << "\n";

    out.close();
    return true;
  }

  for (int i = 0; i < exprPool->size(); i++) {
    NodeStats* score = exprPool->items[i]->ns;

    NodeStats::computeScore(
      blockID,
      genID,
      stepID,
      varID,
      opts,
      dataIn,
      exprPool->items[i]->n,
      score);

    out << i
      << "|" << score->maxAbsError
      << "|" << score->nodeCount
      << "|" << score->depth
      << "|" << score->mae
      << "|" << score->mse
      << "|" << score->rmse
      << "|" << score->psnr
      << "|" << score->numWithinTol
      << "|" << score->numOutsideTol
      << "|" << score->numNaN
      << "|" << score->numInfinity
      << "|" << score->blockSize
      << "|" << exprPool->items[i]->n->canonicalString()
      << "\n";
  }

  out.close();
  return true;
}

size_t serializedNodeSize(const Node* node)
{
  if (node == nullptr)
    return 0;

  // Every node has a one-byte type/operator tag.
  size_t bytes = 1;

  // Coefficient nodes additionally store a 32-bit float.
  if (node->getKind() == NODE_VALUE)
    bytes += sizeof(float);

  bytes += serializedNodeSize(node->getLeftChild());
  bytes += serializedNodeSize(node->getRightChild());

  return bytes;
}

size_t estimateAstBinarySize(const ExprArray& expressions)
{
  constexpr size_t headerBytes = 12;
  constexpr size_t nodeCountBytes = sizeof(uint16_t);

  size_t bytes = headerBytes;

  for (const ExprStats* expression : expressions.items) {
    // Each expression begins with its two-byte node count.
    bytes += nodeCountBytes;

    if (expression != nullptr)
      bytes += serializedNodeSize(expression->n);
  }

  return bytes;
}

int countCoefficientNodes(const Node* node)
{
  if (node == nullptr)
    return 0;

  int count = node->isCoeffNode() ? 1 : 0;

  count += countCoefficientNodes(node->getLeftChild());
  count += countCoefficientNodes(node->getRightChild());

  return count;
}

int serializedExpressionBytes(const ExprStats* es)
{
  if (es == nullptr || es->n == nullptr || es->ns == nullptr)
    return 0;

  // Two-byte node count, one byte per node, and four extra bytes
  // for every coefficient value.
  return 2 +
         es->ns->nodeCount +
         4 * countCoefficientNodes(es->n);
}

// Generation zero is seeded exclusively from BlockCharacterization below.

int main(int argc, char* argv[]) {
  const char* seedText = std::getenv("EXPRGEN_SEED");
  const std::uint64_t masterSeed = seedText != nullptr
      ? std::strtoull(seedText, nullptr, 10)
      : static_cast<std::uint64_t>(time(nullptr));

  printHeader();

  Options opts;

  opts.tol = 0.025;
  opts.maxFloats = INT_MAX;
  opts.maxGenerations = 10;
  opts.saveInterval = 4;
  opts.blockSize = 1024;
  opts.blockOffset = 0;

  int varID = 0; // using x only

  if (processArgs(argc, argv, opts))
    return 1;

  ensureDefaultVariables();

  const std::string inputDataName =
      std::filesystem::path(opts.input_file).stem().string();
  const std::filesystem::path defaultReportDirectory =
      std::filesystem::path("testrpts") / inputDataName;

  if (opts.segments_file.empty()) {
    opts.segments_file =
        (defaultReportDirectory / (inputDataName + ".zsr")).string();
  }
  if (opts.decom_file.empty()) {
    opts.decom_file =
        (defaultReportDirectory / (inputDataName + ".f32")).string();
  }

  const std::filesystem::path segmentsDirectory =
      std::filesystem::path(opts.segments_file).parent_path();
  const std::filesystem::path decomDirectory =
      std::filesystem::path(opts.decom_file).parent_path();
  if (!segmentsDirectory.empty())
    std::filesystem::create_directories(segmentsDirectory);
  if (!decomDirectory.empty())
    std::filesystem::create_directories(decomDirectory);

  logNote("- Input file:        %s", opts.input_file.c_str());
  logNote("- Segment file:      %s", opts.segments_file.c_str());
  logNote("- Output file:       %s", opts.decom_file.c_str());
  logNote("- Tolerance:         %g", opts.tol);

  logNote("- Block size:        %d", opts.blockSize);
  logNote("- Max Floats:        %d", opts.maxFloats);
  logNote("- Max Generations:   %d", opts.maxGenerations);
  logEvolutionRates(opts.evolutionRates);
  logNote("- Save Interval:     %d", opts.saveInterval);
  logNote("- Random Seed:       %llu",
          static_cast<unsigned long long>(masterSeed));

  FILE* fp = fopen(opts.input_file.c_str(), "rb");

  if (fp == 0) {
    logError("Unable to open input file: %s", opts.input_file.c_str());
    return 1;
  }

  fseek(fp, 0, SEEK_END);
  long fileSize = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  long numFloats = fileSize / sizeof(float);

  // let maxFloats dictate how much is read in
  // but then its reused to indicate the numFloats actually read in
  if (numFloats > opts.maxFloats)
    numFloats = opts.maxFloats;
  else {
    opts.maxFloats = numFloats;
  }

  float* dataIn = new float[numFloats];

  size_t count = fread(dataIn, sizeof(float), numFloats, fp);
  fclose(fp);

  logNote("\n\nRead in %zu floats\n\n", count);

  CPUTimer all_timer;
  all_timer.start();

  int numBlocks =
      numFloats == 0 ? 0 : 1 + (numFloats - 1) / opts.blockSize;

  std::vector<ExprArray*> blockPools(numBlocks, nullptr);
  std::vector<ExprStats*> bestExpressions(numBlocks, nullptr);
  std::vector<ExprStats*> fallbackExpressions(numBlocks, nullptr);
  // vector<bool> packs multiple flags into one word, so separate threads
  // cannot safely update distinct elements. Bytes provide independent slots.
  std::vector<std::uint8_t> blockActive(numBlocks, 1);

  ////////////////////////////////////////////////////////
  //
  // The Genetic Algorithm Loop
  //
  ////////////////////////////////////////////////////////

  for (int genID = 0; genID < opts.maxGenerations; genID++) {
    ////////////////////////////////////////////////////////
    //
    // The Block Loop
    //
    ////////////////////////////////////////////////////////

    std::vector<std::string> blockLogs(numBlocks);

    #pragma omp parallel for schedule(dynamic, 1)
    for (int blockID = 0; blockID < numBlocks; blockID++) {
      if (!blockActive[blockID])
        continue;

      ScopedLogCapture blockLogCapture(blockLogs[blockID]);

      // Seeding from logical work IDs makes results independent of which
      // OpenMP worker happens to process this block.
      const std::uint64_t blockGenerationSeed =
          masterSeed ^
          (0x9e3779b97f4a7c15ULL +
           static_cast<std::uint64_t>(blockID) * 0xbf58476d1ce4e5b9ULL +
           static_cast<std::uint64_t>(genID) * 0x94d049bb133111ebULL);
      seedRandom(blockGenerationSeed);

      Options blockOpts = opts;
      blockOpts.blockOffset =
          static_cast<long>(blockID) * opts.blockSize;
      blockOpts.blockSize = static_cast<int>(
          std::min<long>(
              opts.blockSize,
              numFloats - blockOpts.blockOffset));

      ExprArray*& exprPool = blockPools[blockID];
      ExprStats*& bestEs = bestExpressions[blockID];
      ExprStats*& fallbackEs = fallbackExpressions[blockID];

      long stepID = 0;

      // PRINT a Title
      logPrint("### Generation %d of %d -> BlockID: %d (blockSize=%d)",
               genID + 1,
               opts.maxGenerations,
               blockID,
               blockOpts.blockSize);


      ExprArray* newPool = nullptr;

      CPUTimer ga_loop_timer;
      ga_loop_timer.start();

      ///////////////////////////////////////////////////
      // STEP 1: Create or Evolve your Expressions
      // /////////////////////////////////////////////////
      stepID = 10;

      if (genID == 0) {
        const BlockCharacterization characterization = characterizeBlock(
            dataIn, blockOpts.blockOffset, blockOpts.blockSize, blockOpts.tol);

        // Print only the first few blocks, and only once, to keep normal runs
        // readable while exposing enough information to tune the characterizer.
        if (blockID < 3) {
          std::ostringstream diagnostic;
          diagnostic << "Block " << blockID << " families:";
          const int shown = std::min<int>(5, characterization.rankedFamilies.size());
          for (int i = 0; i < shown; ++i) {
            const FamilyScore& family = characterization.rankedFamilies[i];
            diagnostic << " " << curveFamilyName(family.family)
                       << "=" << std::fixed << std::setprecision(3) << family.score;
            if (std::isfinite(family.maxAbsError))
              diagnostic << "(MaxAE " << std::setprecision(4)
                         << family.maxAbsError << ")";
          }
          logNote("%s", diagnostic.str().c_str());
        }

        std::vector<std::string> initialExpressions =
            makeCharacterizedSeeds(characterization);
        if (blockID < 3) {
          logNote("Block %d selected %d characterized first-generation seeds.",
                  blockID, static_cast<int>(initialExpressions.size()));
        }
        exprPool = generateBasicExpressionsFromText(initialExpressions);
        writeCsvExprArrayStats(blockID, genID, stepID+1, varID, blockOpts, dataIn, exprPool);
        ExprSimplifier::simplifyExpressionArray(*exprPool, true);
        writeCsvExprArrayStats(blockID, genID,  stepID+2, varID, blockOpts, dataIn, exprPool);

        ExprArray* uniquePool = removeDuplicateExpressions(exprPool);
        delete exprPool;
        exprPool = uniquePool;
      }
      else {
        do {
          newPool = evolveExpressions(
            exprPool,
            blockID,
            genID,
            stepID,
            varID,
            blockOpts,
            dataIn);
          delete exprPool;
          exprPool = newPool;
        }
        while (exprPool->size() < 20);

        writeCsvExprArrayStats(blockID, genID, stepID+3, varID, blockOpts, dataIn, exprPool);
        ExprSimplifier::simplifyExpressionArray(*exprPool, true);
        writeCsvExprArrayStats(blockID, genID, stepID+4, varID, blockOpts, dataIn, exprPool);

        ExprArray* uniquePool = removeDuplicateExpressions(exprPool);
        delete exprPool;
        exprPool = uniquePool;
      }

      /////////////////////////////////////////////////
      // STEP 2: Optimize Expression Coefficients
      /////////////////////////////////////////////////
      stepID=20;

      //
      // for (int i = 0; i < exprPool->size(); i++) {
      //   ExprStats* es = exprPool->items[i];
      //
      //   optimize_NodeCoeffs_HillClimbing_Search(
      //     blockID,
      //     genID,
      //     stepID,
      //     varID,
      //     opts,
      //     es->n,
      //     dataIn,
      //     1.0,
      //     1000,
      //     1
      //   );
      //
      //   NodeStats::computeScore(
      //     blockID,
      //     genID,
      //     stepID,
      //     varID,
      //     opts,
      //     dataIn,
      //     es->n,
      //     es->ns);
      // }

      printExprArrayStats(blockID, genID, stepID, varID, blockOpts, dataIn, exprPool);

      NodeStats avg = averageNodeStats(*exprPool);

      logPrint("\n- average MaxAbsError: %.1f", avg.maxAbsError);

      /////////////////////////////////////////////////
      // STEP 3: Keep low MaxAbsErr Expressions
      /////////////////////////////////////////////////

      stepID=30;

      ExprSimplifier::simplifyExpressionArray(*exprPool, false);

      // Simplification replaces each AST and can create non-finite constants
      // through constant folding. Refresh the scores so filtering examines the
      // simplified expressions rather than stale pre-simplification results.
      for (ExprStats* es : exprPool->items) {
        if (es == nullptr || es->n == nullptr || es->ns == nullptr)
          continue;

        NodeStats::computeScore(
            blockID,
            genID,
            stepID,
            varID,
            blockOpts,
            dataIn,
            es->n,
            es->ns);
      }

      ExprArray* uniquePool = removeDuplicateExpressions(exprPool);
      delete exprPool;
      exprPool = uniquePool;

      ExprArray* filtered = filterPool(exprPool); // keeps the best half based on MaxAbsError

      delete exprPool;
      exprPool = filtered;

      /////////////////////////////////////////////////
      // STEP 4: Print  and log expression stats
      /////////////////////////////////////////////////

      stepID=40;

      writeCsvExprArrayStats(blockID, genID, stepID, varID, blockOpts, dataIn, exprPool);

      // Stop evolving this block if no expressions survived, while allowing
      // the remaining blocks and generation checkpoint to complete.
      if (printExprArrayStats(
              blockID, genID, stepID, varID, blockOpts, dataIn, exprPool)) {
        blockActive[blockID] = false;
        continue;
      }


      //////////////////////////////////////////////////
      // STEP 5: Record best expression for this blockID
      //////////////////////////////////////////////////

      for (int j = 0; j < exprPool->size(); ++j) {
        ExprStats* es = exprPool->items[j];

        if (es == nullptr || es->n == nullptr || es->ns == nullptr)
          continue;

        // Always retain the lowest-error expression as a fallback in case no
        // expression produced for this block satisfies the requested tolerance.
        if (fallbackEs == nullptr ||
            es->ns->maxAbsError < fallbackEs->ns->maxAbsError) {
          delete fallbackEs;
          fallbackEs = es->clone();
        }

        if (es->ns->maxAbsError >= opts.tol)
          continue;

        const bool smaller =
            bestEs == nullptr ||
            serializedExpressionBytes(es) <
                serializedExpressionBytes(bestEs);

        const bool sameSizeButLowerError =
            bestEs != nullptr &&
            serializedExpressionBytes(es) ==
                serializedExpressionBytes(bestEs) &&
            es->ns->maxAbsError < bestEs->ns->maxAbsError;

        if (smaller || sameSizeButLowerError) {
          delete bestEs;
          bestEs = es->clone();
        }
      }

      logPrint("\n\nGeneration %d: %.6f s\n",
               genID + 1,
               ga_loop_timer.elapsed());
    }

    // OpenMP workers finish in arbitrary order. Emit each complete Markdown
    // section only after the parallel region, indexed by logical block ID.
    for (const std::string& blockLog : blockLogs)
      fputs(blockLog.c_str(), stdout);

    /////////////////////////////////////////////////
    // END Block Loop
    /////////////////////////////////////////////////

    const int generationNumber = genID + 1;
    const bool saveGeneration =
        generationNumber % opts.saveInterval == 0 ||
        generationNumber == opts.maxGenerations;

    if (!saveGeneration)
      continue;

    auto* generationExpressions = new ExprArray();

    for (int blockID = 0; blockID < numBlocks; ++blockID) {
      ExprStats* selected = bestExpressions[blockID] != nullptr
          ? bestExpressions[blockID]
          : fallbackExpressions[blockID];

      if (selected == nullptr) {
        logError("No valid expression was produced for block %d", blockID);
        delete generationExpressions;
        for (ExprArray* pool : blockPools) delete pool;
        for (ExprStats* es : bestExpressions) delete es;
        for (ExprStats* es : fallbackExpressions) delete es;
        delete[] dataIn;
        return 1;
      }

      if (bestExpressions[blockID] == nullptr) {
        logNote(
            "Block %d: no expression met tolerance %g; using lowest-error "
            "fallback with MaxAE %g",
            blockID,
            opts.tol,
            fallbackExpressions[blockID]->ns->maxAbsError);
      }

      generationExpressions->add(selected->clone());
    }

    const auto generationFilename = [generationNumber](
        const std::string& requestedFilename) {
      const std::filesystem::path requested(requestedFilename);
      return (requested.parent_path() /
              (requested.stem().string() + "_gen" +
               std::to_string(generationNumber) +
               requested.extension().string()))
          .string();
    };

    const std::string expressionsFilename =
        generationFilename(opts.segments_file);
    const std::string reconstructedFilename =
        generationFilename(opts.decom_file);

    logPrint("## SAVING generation %d to file: %s",
             generationNumber,
             expressionsFilename.c_str());
    saveExprArrayAstBinary(
        expressionsFilename,
        *generationExpressions,
        opts.blockSize,
        opts.tol,
        2,
        0);

    delete generationExpressions;

    logPrint("## LOADING generation %d from file: %s",
             generationNumber,
             expressionsFilename.c_str());
    AstBinaryHeader loadedHeader{};
    ExprArray* loadedExpressions =
        loadExprArrayAstBinary(expressionsFilename, &loadedHeader);

    printBlockExprArrayStats(
        varID, opts, numFloats, dataIn, loadedExpressions);

    Options generationOpts = opts;
    generationOpts.decom_file = reconstructedFilename;
    if (!reconstructAndAnalyze(
            varID,
            generationOpts,
            dataIn,
            numFloats,
            loadedExpressions)) {
      delete loadedExpressions;
      for (ExprArray* pool : blockPools) delete pool;
      for (ExprStats* es : bestExpressions) delete es;
      for (ExprStats* es : fallbackExpressions) delete es;
      delete[] dataIn;
      return 1;
    }

    delete loadedExpressions;
  }

  logPrint(
      "All blocks %d and all %d generations: %.6f s\n",
      numBlocks,
      opts.maxGenerations,
      all_timer.elapsed());
  logEvolutionOperatorStats();

  for (ExprArray* pool : blockPools) delete pool;
  for (ExprStats* es : bestExpressions) delete es;
  for (ExprStats* es : fallbackExpressions) delete es;
  delete[] dataIn;
  return 0;
}

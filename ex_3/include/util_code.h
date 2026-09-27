#pragma once

#include <cstdint>
#include <string>

struct EvolutionRates {
  double crossoverProbability = 0.20;
  double rotateTree = 4.2600;
  double addUnaryNode = 1.2350;
  double addBinaryNode = 1.2350;
  double deleteNode = 0.8700;
  double mutateOperator = 0.2930;
  double doNothing = 0.2730;
  double swapOperands = 0.1980;
  double mutateConstant = 0.0346;
  double insertAffine = 0.0112;
  double simplify = 0.00209;
  double randomize = 0.000502;
  double optimize = 0.0;

  bool setWeight(const std::string& name, double value);
  double totalMutationWeight() const;
};

struct Options {
  std::string input_file;
  std::string segments_file;
  std::string decom_file;

  double tol;
  long maxFloats;
  int maxGenerations;
  int saveInterval;
  int blockSize;
  long blockOffset;
  EvolutionRates evolutionRates;
};

double randomDouble(double minVal, double maxVal);
double randomDouble(double minVal, double maxVal, double defaultVal);
int randomInt(int upperExclusive);
void seedRandom(std::uint64_t seed);

void logEvolutionRates(const EvolutionRates& rates);
void logEvolutionOperatorStats();

struct CPUTimer;

void logTrace(const char* fmt, ...);
void logPrint(const char* fmt, ...);
void logNote(const char* fmt, ...);
void logWarn(const char* fmt, ...);
void logError(const char* fmt, ...);

// Routes log calls made by the current thread into a caller-owned buffer.
// This is used by parallel block processing so complete Markdown sections can
// be emitted later in deterministic block order.
class ScopedLogCapture {
public:
  explicit ScopedLogCapture(std::string& buffer);
  ~ScopedLogCapture();

  ScopedLogCapture(const ScopedLogCapture&) = delete;
  ScopedLogCapture& operator=(const ScopedLogCapture&) = delete;

private:
  std::string* previousBuffer;
};

void csvWrite(const char* fmt, ...);

#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>

#include <string>
#include <vector>


struct CPUTimer
{
    timeval beg, end;

    CPUTimer();
    ~CPUTimer();

    void start();
    double elapsed();
};

struct Options
{
    const char* inputFile;
    const char* outputFile;
    const char* zipFile;

    double tolerance;
    int maxFloats;
    int maxLoops;
    int maxGenerations;
    int populationSize;

    bool verbose;

    Options() :
        inputFile(nullptr),
        outputFile(nullptr),
        zipFile(nullptr),
        tolerance(0.01),
        maxFloats(1024),
        maxLoops(100),
        maxGenerations(100),
        populationSize(100),
        verbose(false)
    {
    }
};

void printHeader();

int processArgs(
    int argc,
    char* argv[],
    Options& opts);

void ensureDefaultVariables();

std::vector<std::string> makeParserVariableNames();

double randomDouble(double minVal, double maxVal);

void appendFormat(char* buffer, int maxLen, const char* fmt, ...);

void vlogPrint(const char* color, const char* fmt, va_list args);

void logPrint(const char* fmt, ...);
void logNote(const char* fmt, ...);
void logWarning(const char* fmt, ...);
void logError(const char* fmt, ...);
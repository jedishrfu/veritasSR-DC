#pragma once

#include <string>

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
#include <string>
#include <random>
#include <cmath>
#include <iostream>
#include <iomanip>

#include "../include/util_code.h"

#include <fstream>

namespace {
thread_local std::mt19937_64 randomGenerator(std::random_device{}());
thread_local std::string* logCaptureBuffer = nullptr;

void appendVFormat(std::string& output, const char* fmt, va_list args) {
    va_list sizeArgs;
    va_copy(sizeArgs, args);
    const int required = vsnprintf(nullptr, 0, fmt, sizeArgs);
    va_end(sizeArgs);
    if (required <= 0)
        return;

    const std::size_t start = output.size();
    output.resize(start + static_cast<std::size_t>(required) + 1);
    vsnprintf(&output[start], static_cast<std::size_t>(required) + 1, fmt, args);
    output.resize(start + static_cast<std::size_t>(required));
}
}

ScopedLogCapture::ScopedLogCapture(std::string& buffer)
    : previousBuffer(logCaptureBuffer) {
    logCaptureBuffer = &buffer;
}

ScopedLogCapture::~ScopedLogCapture() {
    logCaptureBuffer = previousBuffer;
}

bool EvolutionRates::setWeight(const std::string& name, double value) {
    if (value < 0.0 || !std::isfinite(value)) return false;
    if (name == "rotate_tree") rotateTree = value;
    else if (name == "add_unary_node") addUnaryNode = value;
    else if (name == "add_binary_node") addBinaryNode = value;
    else if (name == "delete_node") deleteNode = value;
    else if (name == "mutate_operator") mutateOperator = value;
    else if (name == "do_nothing") doNothing = value;
    else if (name == "swap_operands") swapOperands = value;
    else if (name == "mutate_constant") mutateConstant = value;
    else if (name == "insert_affine") insertAffine = value;
    else if (name == "simplify") simplify = value;
    else if (name == "randomize") randomize = value;
    else if (name == "optimize") optimize = value;
    else return false;
    return true;
}

double EvolutionRates::totalMutationWeight() const {
    return rotateTree + addUnaryNode + addBinaryNode + deleteNode +
        mutateOperator + doNothing + swapOperands + mutateConstant +
        insertAffine + simplify + randomize + optimize;
}

void logEvolutionRates(const EvolutionRates& rates) {
    const double mutationProbability = 1.0 - rates.crossoverProbability;
    const double total = rates.totalMutationWeight();
    logNote("- Crossover probability: %.4f (%.2f%%)",
            rates.crossoverProbability, 100.0 * rates.crossoverProbability);
    const auto show = [&](const char* name, double weight) {
        const double overall = total > 0.0
            ? 100.0 * mutationProbability * weight / total : 0.0;
        logNote("  %-20s weight=%-9g overall=%6.3f%%", name, weight, overall);
    };
    show("rotate_tree", rates.rotateTree);
    show("add_unary_node", rates.addUnaryNode);
    show("add_binary_node", rates.addBinaryNode);
    show("delete_node", rates.deleteNode);
    show("mutate_operator", rates.mutateOperator);
    show("do_nothing", rates.doNothing);
    show("swap_operands", rates.swapOperands);
    show("mutate_constant", rates.mutateConstant);
    show("insert_affine", rates.insertAffine);
    show("simplify", rates.simplify);
    show("randomize", rates.randomize);
    show("optimize", rates.optimize);
}

void seedRandom(std::uint64_t seed) {
    randomGenerator.seed(seed);
}

int randomInt(int upperExclusive) {
    if (upperExclusive <= 0)
        return 0;

    std::uniform_int_distribution<int> dist(0, upperExclusive - 1);
    return dist(randomGenerator);
}

double randomDouble(double minVal, double maxVal) {
    std::uniform_real_distribution<double> dist(minVal, maxVal);

    return dist(randomGenerator);
};


void appendFormat(char* buffer, int maxLen, const char* fmt, ...) {
    if (buffer ==
        nullptr || maxLen <= 0)
        return;

    int len = static_cast<int>(strlen(buffer));
    if (len >= maxLen - 1)
        return;

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer + len, maxLen - len, fmt, args);
    va_end(args);

    buffer[maxLen - 1] = '\0';
}

void writeCsvText(const std::string& filename, const std::string& text) {
    std::ofstream fout(filename);

    if (!fout) throw std::runtime_error("Cannot open for writing: " + filename);
    fout << std::fixed << std::setprecision(6);
    fout << "# start,end,degree,a0,a1,a2,a3,a4\n";
    fout << text;
    // for (const auto& s : segs) {
    //     fout << s.start << "," << s.end << "," << s.degree;
    //     for (int j=0; j<COEFF_CAP; ++j) fout << "," << s.coeff[j];
    //     fout << "\n";
    // }
}

void vlogPrint(const char* color, const char* fmt, va_list args) {
    if (!fmt)
        return;
    if (!color)
        color = "";

    const bool useColor =
        color[0] != 0 && fmt[0] != '#' && fmt[0] != '-' && fmt[0] != '|';

    if (logCaptureBuffer != nullptr) {
        if (useColor) {
            *logCaptureBuffer += "<span style='color:";
            *logCaptureBuffer += color;
            *logCaptureBuffer += ";'>";
            appendVFormat(*logCaptureBuffer, fmt, args);
            *logCaptureBuffer += "</span>\n\n";
        }
        else {
            appendVFormat(*logCaptureBuffer, fmt, args);
            *logCaptureBuffer += '\n';
        }
        return;
    }

    if (useColor) {
        printf("<span style='color:%s;'>", color);
        vprintf(fmt, args);
        printf("</span>\n\n");
    }
    else {
        vprintf(fmt, args);
        printf("\n");
    }
}

void logTrace(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vlogPrint("darkgreen", fmt, args);
    va_end(args);
}

void logPrint(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vlogPrint("", fmt, args);
    va_end(args);
}

void logNote(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vlogPrint("blue", fmt, args);
    va_end(args);
}

void logWarn(const char* fmt, ...) {
    if (logCaptureBuffer != nullptr)
        *logCaptureBuffer += "\n\nWarning: ";
    else
        printf("\n\nWarning: ");

    va_list args;
    va_start(args, fmt);
    vlogPrint("orange", fmt, args);
    va_end(args);
}

void logError(const char* fmt, ...) {
    if (logCaptureBuffer != nullptr)
        *logCaptureBuffer += "\n\nERROR: ";
    else
        printf("\n\nERROR: ");

    va_list args;
    va_start(args, fmt);
    vlogPrint("red", fmt, args);
    va_end(args);
}

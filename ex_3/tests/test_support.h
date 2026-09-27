#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "expr_array.h"
#include "expr_parser.h"
#include "var_table.h"

// The application owns this thread-local state; each test executable owns its own.
inline thread_local VarTable varTable;

namespace test {
inline void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
inline void near(double actual, double expected, double tolerance,
                 const std::string& message) {
    if (!std::isfinite(actual) || !std::isfinite(expected) ||
        std::fabs(actual - expected) > tolerance)
        throw std::runtime_error(message + ": actual=" + std::to_string(actual) +
                                 ", expected=" + std::to_string(expected));
}
inline std::unique_ptr<Node> parse(const std::string& expression) {
    std::unique_ptr<Node> node(parseExpression(expression, {"x"}));
    require(bool(node), "Cannot parse " + expression + ": " + getLastParseError());
    return node;
}
inline Options options(int size, long offset = 0, double tol = 1e-6) {
    Options result{};
    result.blockSize = size;
    result.blockOffset = offset;
    result.tol = tol;
    return result;
}
inline std::vector<float> samples(int size, const std::function<double(double)>& f) {
    std::vector<float> values;
    for (int i = 0; i < size; ++i) values.push_back(static_cast<float>(f(i)));
    return values;
}
// Independent numerical oracle: does not use NodeStats or the optimizer's score.
inline double maxError(Node& node, const std::vector<float>& data,
                       int size, long offset = 0) {
    double error = 0;
    for (int i = 0; i < size; ++i) {
        varTable.setValue(0, i);
        const double prediction = node.eval();
        require(std::isfinite(prediction), "Non-finite prediction at x=" + std::to_string(i));
        error = std::max(error, std::fabs(prediction - data.at(offset + i)));
    }
    return error;
}
inline int run(int argc, char** argv,
               std::initializer_list<std::pair<const char*, std::function<void()>>> cases) {
    if (argc > 2) { std::cerr << "Usage: " << argv[0] << " [case-name-substring]\n"; return 2; }
    int passed = 0, failed = 0;
    for (const auto& item : cases) {
        if (argc == 2 && std::string(item.first).find(argv[1]) == std::string::npos) continue;
        varTable.resetToDefault();
        seedRandom(12345);
        try { item.second(); ++passed; std::cout << "PASS " << item.first << '\n'; }
        catch (const std::exception& e) {
            ++failed; std::cerr << "FAIL " << item.first << ": " << e.what() << '\n';
        }
    }
    std::cout << passed << " passed, " << failed << " failed\n";
    return failed ? 1 : (passed ? 0 : 2);
}
}

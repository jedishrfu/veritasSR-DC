#include "test_support.h"
#include "block_characterizer.h"

namespace {
constexpr double pi = 3.14159265358979323846;
void family(CurveFamily expected, const std::function<double(double)>& f,
            int size = 64, double tolerance = 1e-3) {
    const auto data = test::samples(size, f);
    const auto result = characterizeBlock(data.data(), 0, size, tolerance);
    test::require(!result.rankedFamilies.empty(), "No family scores");
    test::require(result.rankedFamilies.front().family == expected,
        std::string("Expected ") + curveFamilyName(expected) + ", got " +
        curveFamilyName(result.rankedFamilies.front().family));
    double previous = 1;
    bool hasFallback = false;
    for (const auto& candidate : result.rankedFamilies) {
        test::require(std::isfinite(candidate.score) && candidate.score >= 0 &&
                      candidate.score <= previous, "Scores must be finite, bounded and ranked");
        previous = candidate.score;
        hasFallback |= candidate.family == CurveFamily::GeneralSymbolic;
    }
    test::require(hasFallback, "Missing general symbolic fallback");
    const auto seeds = makeCharacterizedSeeds(result, 3);
    test::require(!seeds.empty() && seeds.size() <= 3, "Seed count outside limit");
    for (const auto& seed : seeds) {
        auto node = test::parse(seed);
        const double error = test::maxError(*node, data, size);
        test::require(std::isfinite(error), "Seed must evaluate finitely");
    }
    auto best = test::parse(result.rankedFamilies.front().seedExpression);
    // Piecewise fitting reports a hard split; its AST is a logistic approximation.
    test::require(result.rankedFamilies.front().maxAbsError <= tolerance,
                  "Best family fit exceeds tolerance");
    // For this continuous V-shaped fixture, permit smoothing error up to 10%
    // of the range; a logistic seed is not the exact hard-split model.
    const double seedTolerance = expected == CurveFamily::PiecewiseLinear
        ? 0.1 * result.dataRange : tolerance;
    test::require(test::maxError(*best, data, size) <= seedTolerance,
                  "Best seed does not reconstruct the synthetic curve");
}
void offset() {
    auto data = test::samples(32, [](double x) { return 3*x + 2; });
    std::vector<float> padded(7, -9999); padded.insert(padded.end(), data.begin(), data.end());
    padded.push_back(9999);
    auto a = characterizeBlock(data.data(), 0, 32, 1e-3);
    auto b = characterizeBlock(padded.data(), 7, 32, 1e-3);
    test::near(b.dataRange, 93, 1e-12, "Block range excludes padding");
    test::near(b.standardDeviation, 3*std::sqrt((32*32-1)/12.0), 1e-10, "Population standard deviation");
    test::require(makeCharacterizedSeeds(a) == makeCharacterizedSeeds(b), "Offset changed local-x seeds");
}
void limits() {
    float value = 2;
    test::require(characterizeBlock(nullptr, 0, 10, 0.01).rankedFamilies.empty(), "Null data");
    test::require(characterizeBlock(&value, 0, 0, 0.01).rankedFamilies.empty(), "Empty data");
    test::require(characterizeBlock(&value, 0, -1, 0.01).rankedFamilies.empty(), "Negative size");
    auto one = characterizeBlock(&value, 0, 1, 0.01);
    test::require(one.rankedFamilies.front().family == CurveFamily::Constant, "Single sample");
    test::require(makeCharacterizedSeeds(one, 0).empty(), "Zero seed limit must return no seeds");
    test::require(makeCharacterizedSeeds(one, -1).empty(), "Negative seed limit must return no seeds");
    test::require(makeCharacterizedSeeds(BlockCharacterization{}).empty(), "Empty characterization");
}
void fallback() {
    BlockCharacterization c;
    c.tolerance = 0.01;
    c.rankedFamilies = {{CurveFamily::GeneralSymbolic, 0.10, INFINITY, ""},
                        {CurveFamily::Linear, 0.05, 2, "x"}};
    test::require(makeCharacterizedSeeds(c) == std::vector<std::string>{"x"}, "Low confidence fallback");
}
}
int main(int argc, char** argv) {
    return test::run(argc, argv, {
        {"constant", [] { family(CurveFamily::Constant, [](double) { return 2.5; }); }},
        {"linear", [] { family(CurveFamily::Linear, [](double x) { return 3*x+2; }); }},
        {"quadratic", [] { family(CurveFamily::Quadratic, [](double x) { return .125*x*x-2*x+3; }); }},
        {"cubic", [] { family(CurveFamily::Cubic, [](double x) { return .015625*x*x*x-.5*x*x+2*x+1; }); }},
        {"sinusoid", [] { family(CurveFamily::Sinusoid, [](double x) { return 2+3*std::sin(2*pi*5*x/64+.3); }); }},
        {"sinusoid_trend", [] { family(CurveFamily::SinusoidTrend, [](double x) { return 2+.1*x+3*std::sin(2*pi*5*x/64+.3); }); }},
        {"two_sinusoids", [] { family(CurveFamily::TwoSinusoids, [](double x) { return 2+3*std::sin(2*pi*5*x/64)+2*std::sin(2*pi*11*x/64+.2); }); }},
        {"exponential", [] { family(CurveFamily::Exponential, [](double x) { return 2+3*std::exp(2*x/63); }); }},
        {"piecewise_linear", [] { family(CurveFamily::PiecewiseLinear, [](double x) { return x<20 ? x : 40-x; }, 40); }},
        {"block_offset_and_statistics", offset}, {"invalid_and_short_inputs", limits},
        {"low_confidence_fallback", fallback}
    });
}

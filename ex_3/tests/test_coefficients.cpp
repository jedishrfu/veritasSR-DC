#include "test_support.h"

namespace {
void fit(const std::string& expression, const std::function<double(double)>& truth,
         double errorLimit, double initialStep = 1, long offset = 0) {
    auto data = test::samples(16, truth);
    data.insert(data.begin(), offset, -9999);
    auto node = test::parse(expression);
    auto opts = test::options(16, offset, 1e-7);
    const double before = test::maxError(*node, data, 16, offset);
    const double reported = optimize_NodeCoeffs_HillClimbing_Search(
        0, 0, 0, 0, opts, node.get(), data.data(), initialStep, 1000, 0);
    const double after = test::maxError(*node, data, 16, offset);
    std::cout << "  error " << before << " -> " << after << '\n';
    test::near(reported, after, 1e-10, "Returned score must match final coefficients");
    test::require(after <= before + 1e-10, "Optimization worsened the model");
    test::require(after <= errorLimit, "Fit missed known synthetic target");
}
void invalid() {
    auto node = test::parse("1"); float value = 1;
    auto opts = test::options(1);
    test::require(optimize_NodeCoeffs_HillClimbing_Search(0,0,0,0,opts,nullptr,&value) >= 1e90, "Null node");
    test::require(optimize_NodeCoeffs_HillClimbing_Search(0,0,0,0,opts,node.get(),nullptr) >= 1e90, "Null data");
    opts.blockSize = 0;
    test::require(optimize_NodeCoeffs_HillClimbing_Search(0,0,0,0,opts,node.get(),&value) >= 1e90, "Empty block");
}
}
int main(int argc, char** argv) {
    return test::run(argc, argv, {
        {"constant_positive", [] { fit("0", [](double) { return 3; }, 1e-6); }},
        {"constant_negative", [] { fit("0", [](double) { return -2.5; }, 1e-6); }},
        {"substep_coefficient", [] { fit("0", [](double) { return .25; }, 1e-6); }},
        {"negative_substep_coefficient", [] { fit("0", [](double) { return -.25; }, 1e-6); }},
        {"linear_slope", [] { fit("1*x", [](double x) { return 3*x; }, 1e-5); }},
        {"linear_intercept", [] { fit("x+0", [](double x) { return x+2; }, 1e-5); }},
        {"two_coefficients", [] { fit("1*x+1", [](double x) { return 3*x+3; }, 1e-5); }},
        {"quadratic_coefficient", [] { fit("1*x*x", [](double x) { return 2*x*x; }, 1e-5); }},
        {"sinusoid_amplitude", [] { fit("1*sin(x)", [](double x) { return 3*std::sin(x); }, 1e-5); }},
        {"intrinsic_argument", [] { fit("sin(x+0)", [](double x) { return std::sin(x+.25); }, 1e-5, .25); }},
        {"nested_intrinsic_groups", [] { fit("sin(exp(x*0))", [](double x) { return std::sin(std::exp(.03125*x)); }, 1e-5, .03125); }},
        {"block_offset", [] { fit("0", [](double) { return 4; }, 1e-6, 1, 7); }},
        {"already_exact", [] { fit("2*x+3", [](double x) { return 2*x+3; }, 0); }},
        {"no_coefficients", [] { fit("x", [](double x) { return x; }, 0); }},
        {"invalid_inputs", invalid}
    });
}

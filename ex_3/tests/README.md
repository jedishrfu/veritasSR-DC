# ex_3 component tests

These standalone C++ programs link the same ex3_core library as ex3_exprgen.
No external testing package, input dataset, or generated report is required.
Each program prints PASS/FAIL per case, then returns 0 for success, 1 for a
failed check, or 2 for invalid arguments/no matching cases. Checks remain active
in Release builds. All inputs are synthetic; random evolution is explicitly seeded.

| CLion target | Coverage |
| --- | --- |
| ex3_test_characterizer | Constant, linear, quadratic, cubic, sinusoid, sinusoid with trend, two sinusoids, exponential, and piecewise linear data; ranked scores; parseable/evaluable seeds; offset handling and statistics; empty/single-sample data; seed limits and fallback |
| ex3_test_evolver | All 13 operator choices; retained parents; tree arity and independent ownership; exact same-seed replay; duplicate removal; coefficient optimization; finite candidate selection and ranking; null/empty pools |
| ex3_test_coefficients | Positive/negative constants, sub-step adjustments, slope/intercept and two-parameter fits, quadratic terms, sinusoid amplitude, intrinsic/nested arguments, offsets, exact fits, coefficient-free trees and invalid inputs |

## CLion

Reload CMake and select any target above with the debug profile. Set breakpoints
in the test or production component and click Debug. Build before launch is enabled.
Use the asan profile for memory error detection. To run one case, put its name
(or a substring) in Program arguments, such as `substep_coefficient` or `crossover`.
With no arguments the program runs every case in its component.

## Command line

Run from the repository root:

    cmake --preset debug
    cmake --build --preset debug --parallel
    ctest --preset debug

To run only component suites:

    ctest --preset debug -L components

To debug a single case directly:

    build/debug/bin/ex3_test_coefficients substep_coefficient

Repeat configure/build/test with `asan` or `release` for those profiles.
The shared build helper also supports the test targets:

    sh scripts/build-experiment.sh asan ex3_test_characterizer ex3_test_evolver ex3_test_coefficients
    ctest --preset asan -L components

CMake/CTest 3.21 or newer is required. Set BUILD_TESTING=OFF to omit tests.
CTest enforces a 60-second timeout for each suite. Same-seed replay is checked
within a run; it does not require an identical random sequence across platforms.

## What the checks mean

Coefficient checks compare the returned error against independent AST evaluation
on known synthetic samples, require that optimization never worsens the fit, and
check a numerical error bound. They do not claim global convergence of hill climbing.

Characterizer tests require the expected family to rank first for well-separated
synthetic curves. Piecewise fits report hard-split error while the emitted AST uses
a smooth logistic transition. Its continuous V-shaped fixture therefore checks the
hard-split fit tightly and allows smoothing error up to 10% of the data range.
Frequency fixtures use known spectral bins; arbitrary-frequency/noisy model selection
and large-dataset performance are not covered by these component tests.

Evolution tests force each operator using its weights, check deterministic replay
and independent trees, and verify that the optimize operator improves a known slope.
Finite-value filtering is tested with NaN/Infinity coefficients and exponential overflow.
All tests run in one thread per process; parallel scheduling behavior is not covered.

## Regression fixes covered

- A nonpositive maximum family count must return zero characterized seeds.
- If neither full-size coefficient probe improves a fit, the search must continue
  with smaller steps. A constant target of +/-0.25 starting at zero with step 1
  previously remained stuck at zero.

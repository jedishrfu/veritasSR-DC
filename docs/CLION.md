# Building and debugging the experiments

Open the repository root in CLion, rather than an individual experiment folder.
Reload CMake after pulling these changes. In Settings > Build, Execution,
Deployment > CMake, enable the imported debug, release, and asan preset profiles.
Select a shared run configuration (for example ex3_exprgen) and the debug profile,
set a breakpoint, and choose Debug. Build before launch is enabled in each shared
configuration, so source changes rebuild automatically.

The old veritasSR_DC run configuration may remain in your personal workspace;
select ex3_exprgen instead. Personal workspace settings are not overwritten.

## Targets

| Target | Program |
| --- | --- |
| ex0_pwpr | Piecewise polynomial regression |
| ex0_cheb_pwpr | Chebyshev variant |
| ex0_readfloats | Text x/y pair reader |
| ex0_spdp | SPDP C program |
| ex2_exprgen | Earlier expression generator |
| ex2_test_coeff_fit | Coefficient fitting experiment |
| ex3_exprgen | Current expression generator |

ex_1 contains Python programs. Continue using its runexprs launcher from ex_1;
it is not a native CMake executable. The ex0 configurations need arguments/input
appropriate to the selected utility before running; their source files describe
the command syntax. readfloats expects a text file containing x/y pairs, not a binary .f32 file.
SPDP uses standard input and writes binary data to stdout; configure input
redirection and use a terminal when saving its binary output.

## Profiles and reproducible runs

- debug: symbols and an unoptimized build for stepping through code.
- release: optimized build for performance measurements.
- asan: Debug plus AddressSanitizer and frame pointers.

All executables live in build/<preset>/bin. The shared ex3 run configuration uses
ex_3 as its working directory and these arguments:

    -b 250 -g 100 -n 1000 -i ../50k_data/sin_x_50k.f32

It sets EXPRGEN_SEED=12345 and OMP_NUM_THREADS=1 to simplify repeatable debugging.
Change the thread count for performance experiments. Outputs use ex_3/testrpts;
repeat runs of the same input can overwrite the default output files. For separate
results, change output arguments or use a different working directory with an
absolute input path. ex2_exprgen uses the existing ps_data_50k.f32 sample.

## Command line

From the repository root (CMake 3.21 or newer):

    cmake --preset debug
    cmake --build --preset debug --parallel
    cmake --build --preset debug --target ex3_exprgen

Or use the helper, which also locates bundled CLion CMake when cmake is absent:

    sh scripts/build-experiment.sh debug ex3_exprgen

Use CMAKE=/absolute/path/to/cmake with the helper to select another CMake binary.
The build is incremental. Use --clean-first with cmake --build only when needed.

ex_3/runcode and ex_2/runcpp now build via CMake (asan by default); set
SR_BUILD_PRESET=debug or release to override. ex_3/runcode preserves its existing
arguments and copies the built executable to the requested legacy path because
batch scripts still use ex_3/exprgen. Run it from ex_3 as before.
ex0's makebins.zsh builds the four ex0 targets with release by default; it no longer
copies executables into /opt/homebrew/bin or refers to the missing pwlr source.

## Dependencies and maintenance

C++17 and a C compiler are required. ex3 requires OpenMP; on macOS the CMake setup
locates Homebrew libomp. If necessary install it with brew install libomp, or set
OpenMP_ROOT to your runtime prefix in a local CMakeUserPresets.json or CMake option.
Do not commit machine-specific paths. Use CLion's Apple Clang/LLDB toolchain on Mac.

CMakeLists.txt is the authoritative source list and dependency definition. Add new
sources explicitly in the corresponding experiment CMakeLists.txt. Keep one main()
per executable, headers scoped to each target, and old-code files out of targets.
SR_BUILD_EX0, SR_BUILD_EX2, and SR_BUILD_EX3 can disable individual experiment groups.
Commit CMakePresets.json and .run/*.run.xml; keep build outputs and personal presets
out of version control.

## ex_3 component tests

Three standalone test programs are available as shared CLion run/debug configurations:
`ex3_test_characterizer`, `ex3_test_evolver`, and `ex3_test_coefficients`. Select
one with the debug or asan profile, then Run or Debug. No data files or program
arguments are needed. See [the component test guide](../ex_3/tests/README.md).

After building a preset, run all registered tests with `ctest --preset debug`
(or release/asan). Tests are enabled by default; `-DBUILD_TESTING=OFF` disables
them. CTest does not build first, so rebuild after source changes.

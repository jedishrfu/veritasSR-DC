# Help FindOpenMP locate Homebrew's keg-only runtime on macOS.
# A caller-supplied OpenMP_ROOT takes precedence.
if(APPLE AND NOT DEFINED OpenMP_ROOT AND NOT DEFINED ENV{OpenMP_ROOT})
    find_program(SR_HOMEBREW_EXECUTABLE brew HINTS /opt/homebrew/bin /usr/local/bin)
    if(SR_HOMEBREW_EXECUTABLE)
        execute_process(COMMAND "${SR_HOMEBREW_EXECUTABLE}" --prefix libomp
            OUTPUT_VARIABLE SR_LIBOMP_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE SR_LIBOMP_RESULT ERROR_QUIET)
        if(SR_LIBOMP_RESULT EQUAL 0)
            set(OpenMP_ROOT "${SR_LIBOMP_PREFIX}")
        endif()
    endif()
endif()
find_package(OpenMP REQUIRED COMPONENTS CXX)

add_library(ex3_core STATIC
    src/ast_binary_io.cpp
    src/ast_nodes.cpp
    src/ast_nodestats.cpp
    src/base_exprs.cpp
    src/best_fit_coeffs.cpp
    src/block_characterizer.cpp
    src/evolve_exprs.cpp
    src/expr_array.cpp
    src/expr_parser.cpp
    src/expr_simplifier.cpp
    src/save-load-ast.cpp
    src/util_code.cpp
    src/var_table.cpp
)
target_include_directories(ex3_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(ex3_core PUBLIC sr_build_options OpenMP::OpenMP_CXX)

add_executable(ex3_exprgen src/expr_main.cpp)
target_link_libraries(ex3_exprgen PRIVATE ex3_core)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

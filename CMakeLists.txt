cmake_minimum_required(VERSION 4.2)
project(veritasSR_DC)

set(CMAKE_CXX_STANDARD 14)

include_directories(ex_3)

add_executable(veritasSR_DC
        ex_3/src/save-load-ast.cpp
        ex_3/src/ast_nodes.cpp
        ex_3/include/ast_nodes.h
        ex_3/src/ast_nodestats.cpp
        ex_3/include/ast_nodestats.h
        ex_3/src/base_exprs.cpp
        ex_3/src/best_fit_coeffs.cpp
        ex_3/src/evolve_exprs.cpp
        ex_3/src/expr_array.cpp
        ex_3/include/expr_array.h
        ex_3/src/expr_main.cpp
        ex_3/CODE_ARCHIVE/old_expr_parser.cpp
        ex_3/src/util_code.cpp
        ex_3/include/util_code.h
        ex_3/src/var_table.cpp
        ex_3/include/var_table.h
        ex_3/include/expr_parser.h
        ex_3/src/expr_parser.cpp)
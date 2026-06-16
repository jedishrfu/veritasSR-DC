Analyze the attached C++ project and perform a complete source-code reorganization.

Goals:
1. Preserve all functionality and behavior.
2. Do not remove any working code.
3. Reduce file size by splitting large source files into logically organized modules.
4. Create a professional directory structure suitable for an open-source research project.

Tasks:
A. Analyze the existing codebase
- Identify all classes, structs, enums, global variables, functions, templates, and source files.
- Produce a dependency map showing which files depend on which headers.
- Detect duplicate code, dead code, unused functions, redundant declarations, and circular dependencies.
- Identify opportunities for improved encapsulation.

B. Reorganize the project
Create a structure similar to:
project/
├── Makefile
├── README.md
├── include/
├── src/
├── tests/
├── data/
├── docs/
└── build/

Further subdivide source code into modules such as include/ast, include/parser, include/evolution, include/fitness, include/storage, include/optimization, include/util and matching src subdirectories.

C. Split large files
If a source file exceeds approximately 300–500 lines, break it into smaller files organized by responsibility, move declarations into headers, move implementations into corresponding .cpp files, minimize header dependencies, and use forward declarations where appropriate.

D. Generate build system
Create a Makefile with automatic dependency generation, debug build, release build, clean target, and test target.

E. Preserve interfaces
Existing public APIs should remain functional. If a function must move, update all includes and references without breaking external callers.

F. Produce documentation
Generate a new directory tree, file-by-file responsibility summary, include dependency diagram, build instructions, and migration notes.

G. Generate deliverables
Output the complete reorganized source tree, every new .h file, every new .cpp file, updated Makefile, and ZIP archive layout.

Important constraints:
- Do not invent functionality.
- Do not delete code unless it is provably unused.
- Keep the project compiling throughout the refactor.
- Preserve comments and copyright notices.
- If code is ambiguous, explain assumptions.
- Prefer composition over inheritance.
- Minimize global variables.
- Minimize compile-time dependencies.

VeritasSR-specific requirements:
- Split exprgen.cpp into mutation, crossover, scoring, population management, and generation modules.
- Split ast_nodes.h into AST declarations plus separate evaluation, serialization, and statistics files when safe.
- Move the infix parser into its own parser subsystem.
- Create a coefficient-optimization module.
- Create a storage module for AST binary/text serialization.

H. Documentation and Copyright Requirements
Every generated source file must contain a professional file header:
/*
 * VeritasSR
 * Symbolic Regression and Equation Discovery System
 * Copyright (c) 2026 James M. McArdle
 * All Rights Reserved.
 * File: <filename>
 * Purpose: <purpose>
 * Author: James M. McArdle
 * SPDX-License-Identifier: Proprietary
 */
For open-source repositories, alternatively support SPDX-License-Identifier: MIT or Apache-2.0 depending on the selected license.

I. Function and Method Documentation
Every public function, class, struct, enum, constructor, destructor, and significant private method must be documented using Doxygen-compatible comments. Include purpose, parameters, return values, errors, complexity, and usage examples.

J. Class Documentation
Every class must contain a class-level description including supported node/object types, ownership, invariants, and thread safety.

K. Usage Examples
Every major API should contain at least one example.

L. Error Conditions
Document invalid parameters, null pointer inputs, numeric overflow, domain errors, divide-by-zero, allocation failures, parsing failures, and file I/O failures.

M. Algorithm Documentation
For nontrivial algorithms provide purpose, inputs, outputs, time complexity, space complexity, and references.

N. Module-Level Documentation
Each subsystem should begin with a module overview describing responsibilities and primary entry points.

O. Generate Developer Documentation
Generate Doxygen-ready comments, UML class diagrams, call graph diagrams, module dependency diagrams, build instructions, and API reference documentation.

P. Architecture Preservation
Before generating files, identify the current architecture, file responsibilities, why each split improves maintainability, and justify every new module. Do not split files merely to reduce line count.

Q. Ownership and Memory Management Audit
Analyze who allocates, owns, and deletes objects. Document shared ownership, leaks, double-delete risks, dangling pointer risks, and caller responsibilities.

R. Modernization Recommendations
Generate a separate report describing std::unique_ptr, std::shared_ptr, std::vector, const correctness, noexcept opportunities, enum class conversions, and move semantics. Do not automatically apply unless safe.

S. Unit Tests
Create tests for AST evaluation, parser correctness, mutation, crossover, serialization, and coefficient optimization.

T. Compilation Validation
Verify every referenced function, class, include, declaration, and definition. Verify there are no circular includes and that the Makefile compiles all files. Do not invent APIs.

U. Symbolic Regression Documentation
Document AST Representation, Expression Generation, Mutation, Crossover, Selection, Fitness Scoring, Coefficient Optimization, Parsing, and Serialization.

V. API Stability
Existing public APIs must remain functional. Preserve signatures, behavior, return values, and error semantics. Generate compatibility wrappers if required.

W. Documentation Artifacts
Generate docs/architecture.md, class_hierarchy.md, ast_design.md, parser_design.md, evolution_design.md, coefficient_optimization.md, serialization.md, build_instructions.md, developer_guide.md.

X. GitHub Publication Readiness
Generate README.md, LICENSE, CONTRIBUTING.md, CODE_OF_CONDUCT.md, and CHANGELOG.md.

Y. Refactoring Report
Produce docs/refactoring_report.md with original file sizes, new file sizes, functions moved, classes moved, dead code detected, duplicates detected, circular dependencies removed, directory structure, build changes, risks, and future improvements.

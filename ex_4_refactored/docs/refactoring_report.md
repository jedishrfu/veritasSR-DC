# Refactoring Report

## What changed

- Normalized uploaded filenames to stable include names.
- Created `include/`, `src/`, `tests/`, `docs/`, `data/`, and `build/`.
- Split the former monolithic `exprgen.cpp` into smaller logical source fragments.
- Added a Makefile with release, debug, test, and clean targets.
- Added the expanded LLM refactoring prompt at `docs/llm_refactor_prompt.md`.

## Important implementation choice

The split source files use `.inc` fragments included by `src/main.cpp`. This keeps the current single-translation-unit semantics, including existing `static` helper functions, while improving navigation. A deeper refactor can later convert these into separately compiled `.cpp` files with explicit headers.

## Risks

- This pass preserves the prototype design rather than modernizing ownership.
- `ast_nodes.h` remains largely header-only.
- Raw-pointer ownership is unchanged.

## Recommended next steps

- Move AST method implementations from `ast_nodes.h` into `src/ast/*.cpp`.
- Replace raw owning pointers with `std::unique_ptr` where safe.
- Add parser, mutation, crossover, and scoring unit tests.

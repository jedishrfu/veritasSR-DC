# Architecture

## Main subsystems

- AST representation: `include/ast_nodes.h`
- Infix parser: `include/infix_parsing_code.h`
- Logging: `include/logging_code.h`
- Initial generation: `src/generation/basic_generation.inc`
- Mutation and crossover: `src/evolution/`
- Fitness/filtering/arguments: `src/fitness/scoring_and_args.inc`
- Program entry point: `src/main.cpp`

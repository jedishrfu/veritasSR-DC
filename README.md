# VeritasSR

Evolutionary Symbolic Regression and Equation Discovery in C++

⸻

## Overview

VeritasSR is a research-oriented symbolic regression system written in C++ that searches for mathematical expressions capable of approximating numerical datasets.

Unlike conventional machine learning models that learn large collections of parameters inside opaque structures, symbolic regression attempts to discover compact, interpretable equations that directly describe observed data.

VeritasSR represents candidate equations as Abstract Syntax Trees (ASTs) and uses a genetic programming style evolutionary search to generate, mutate, evaluate, and select expressions.

The current implementation focuses on:

* Equation discovery
* Symbolic regression research
* Evolutionary search techniques
* Expression complexity analysis
* Scientific data exploration
* Interpretable mathematical models

⸻

## Why Symbolic Regression?

Traditional regression typically assumes a predefined model form:

y = ax + b

or

y = ax² + bx + c

Symbolic regression removes this assumption and attempts to discover both:

1. The structure of the equation
2. The numerical coefficients

directly from data.

For example, given data generated from:

y = 3.5 * sin(x) + 2

the goal is to recover an equivalent symbolic expression automatically.

⸻

## Features

Current implementation features include:

* AST-based expression representation
* Evolutionary symbolic regression
* Genetic programming style mutation
* Expression cloning and tree manipulation
* Random coefficient optimization
* ~~Infix expression parser~~
* ~~Multiple variable support~~
* Human-readable equation output
* Expression complexity measurement
* Floating-point dataset evaluation
* ~~Binary dataset input~~
* Automatic fitness scoring
* Error-based filtering and selection

⸻

## Architecture

graph TD
Data --> Parser
Parser --> AST
AST --> Evolution
Evolution --> Scoring
Scoring --> Selection
Selection --> Evolution
Selection --> BestEquations

### Node

The Node class is the fundamental building block of every expression.

A node may represent:

* Constant value
* Variable
* Unary operator
* Binary operator

Each node supports:

* Evaluation
* Cloning
* String conversion
* Coefficient access
* Tree traversal

⸻

### ExprArray

ExprArray stores collections of candidate expressions.

Responsibilities:

* Population storage
* Expression ownership
* Population management
* Evolution input/output

⸻

### ExprStats

ExprStats combines:

Node*
NodeStats*

into a single structure representing:

* An expression
* Its evaluation statistics

⸻

### NodeStats

Stores performance metrics for an expression.

Examples:

* MSE
* RMSE
* MAE
* PSNR
* Node count
* Tree depth

⸻

### Parser

~~The infix parser converts expressions such as:

sin(x) + 3

into AST structures.

The parser currently supports:

* Parentheses
* Unary minus
* Variables
* Constants
* Function calls
* Operator precedence~~

⸻

## Evolution Engine

The evolutionary engine is implemented primarily in:

generateBasicExpressions()
evolveExpressions()
filterPool()

Responsibilities:

* Generate seed expressions
* Apply mutations
* Evaluate fitness
* Select survivors

⸻

## Genetic Algorithm Workflow

flowchart TD
A[Generate Initial Expressions]
--> B[Assign Random Coefficients]
B --> C[Evaluate Expressions]
C --> D[Compute Error Metrics]
D --> E[Filter by Cutoff]
E --> F[Mutate Survivors]
F --> G[Create New Population]
G --> H[Next Generation]
H --> C

1. Initial Population

The system generates basic expressions for each variable:

sin(x)
cos(x)
exp(x)
log(x)
x + c
x - c
x * c
x / c

⸻

2. Evaluation

Expressions are evaluated across all samples in the dataset.

⸻

3. Error Calculation

Fitness statistics are computed.

⸻

4. Complexity Measurement

Structural complexity is measured using:

* Node count
* Tree depth

⸻

5. Selection

Expressions exceeding the current cutoff score are discarded.

⸻

6. Mutation

Several mutation operators are implemented.

⸻

7. Population Growth

Surviving expressions are expanded through mutation.

⸻

8. Replacement

The previous population is replaced by the new generation.

⸻

9. Convergence

The cutoff threshold becomes progressively stricter:

cutoffScore *= 0.5;

allowing increasingly accurate expressions to survive.

⸻

## Expression Representation

### Constants

3.14

represented by:

NODE_VALUE

⸻

### Variables

x

represented by:

NODE_VARIABLE

⸻

### Unary Expressions

sin(x)

represented as:

    sin
     |
     x

⸻

### Binary Expressions

x + 3

represented as:

      +
     / \
    x   3

⸻

### Supported Operators

Category	Operators
Arithmetic	+, -, *, /
Trigonometric	sin, cos
Exponential	exp
Logarithmic	log
Other	unary minus

⸻

## Mutation Operators

The current implementation includes:

### Constant Mutation

Randomly perturbs coefficient values.

oldValue + random(-1,+1)

⸻

### Operator Mutation

Changes an operator.

Example:

x + 3

may become

x * 3

⸻

### Unary Wrapping

Wraps an expression inside:

sin(...)
cos(...)
exp(...)
log(...)

⸻

### Binary Combination

Combines two existing expressions.

Example:

sin(x)
+
exp(x)

⸻

### Node Deletion

Randomly removes part of a tree to simplify an expression.

⸻

## Scoring and Fitness

The implementation computes:

Metric	Description
MAE	Mean Absolute Error
MSE	Mean Squared Error
RMSE	Root Mean Squared Error
PSNR	Peak Signal-to-Noise Ratio
Max Absolute Error	Largest observed error
Node Count	Expression size
Tree Depth	Expression depth
Within Tolerance	Samples meeting error threshold
Outside Tolerance	Samples exceeding threshold

Current filtering is based primarily on:

MSE < cutoffScore

⸻

Input Data

Input data is expected to be:

32-bit floating point values

stored in binary format:

*.f32

Example:

obs_temp.f32

⸻

Building

Current build requirements:

* C++17-compatible compiler
* Standard Library
* POSIX getopt support

Example:

git clone https://github.com/yourname/VeritasSR.git
cd VeritasSR
g++ -std=c++17 \
    exprgen.cpp \
    -O3 \
    -o veritassr

⸻

## Running

Basic execution:

./veritassr -i data.f32

Specify tolerance:

./veritassr \
    -i data.f32 \
    -e 0.01

Limit dataset size:

./veritassr \
    -i data.f32 \
    -n 1000

Specify generations:

./veritassr \
    -i data.f32 \
    -g 20

Command-Line Options

Option	Meaning
-i	Input data file
-z	Segment file
-o	Output file
-e	Error tolerance
-n	Maximum floats to read
-g	Maximum generations

⸻

## Example Output

Example evolved expressions:

sin(x)
(x + 4.73)
exp(sin(x))
(log(x) + 2.18)
((sin(x) * 1.7) + 0.42)

Actual results depend on:

* Dataset
* Generation count
* Random seed
* Fitness cutoff

⸻

## Research Applications

Potential applications include:

### Equation Discovery

Recover governing equations directly from data.

### Scientific Modeling

Approximate observed phenomena with symbolic expressions.

### Physics

Explore analytical approximations for experimental datasets.

### Engineering

Construct compact surrogate models.

### Data Compression Research

Represent large numerical datasets as equations instead of samples.

### Explainable AI

Generate interpretable models rather than black-box predictors.

⸻

## Current Limitations

Current implementation intentionally remains simple.

Notable limitations include:

* No crossover operator yet
* No subtree swapping
* No gradient-based coefficient optimization
* No Pareto optimization
* No parallel evaluation
* No GPU acceleration
* No power operator
* No symbolic simplification
* MSE-only survivor selection

⸻

## Roadmap

Possible future directions:

* Subtree crossover
* Multi-objective optimization
* Parallel fitness evaluation
* GPU acceleration
* Additional operators
* Symbolic simplification
* Better coefficient optimization
* Automatic constant fitting
* Distributed search
* Scientific benchmark suite

These items are future possibilities and are not currently implemented.

⸻

## Citation

@software{veritassr,
  title={VeritasSR},
  author={McArdle, James},
  year={2026},
  url={https://github.com/<username>/VeritasSR},
  note={Evolutionary symbolic regression and equation discovery system}
}

⸻

## Areas of interest include:

* New mutation operators
* Crossover implementations
* Optimization methods
* Performance improvements
* Additional mathematical operators
* Benchmark datasets
* Documentation improvements

==This project is currently NOT accepting:==

* Bug reports
* Feature requests
* Pull requests

⸻

## License

License selection is pending.

Until a license is added, all rights remain reserved by the author.

⸻

## Acknowledgements

VeritasSR is inspired by research in:

* Genetic Programming
* Symbolic Regression
* Equation Discovery
* Scientific Computing
* Evolutionary Algorithms

Related systems include:

* Eureqa
* PySR
* AI Feynman

No code from those projects is included in this repository.

# PySR block fitting for ex_3 .zsr files

Use Python 3.10 or later. In this directory:

```sh
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
python pysr_to_zsr.py /Users/jamesmcardle/Desktop/ECL-Projects/active-projects/veritasSR-DC/50k_data/ps_data_50k.f32 -b 1024 -e 0.025 --iterations 100 -o ps_data_pysr.zsr
```

PySR uses Julia. Its first import/run may download and initialize Julia and packages,
requiring internet access and extra startup time. Run a small trial first:

```sh
python pysr_to_zsr.py /Users/jamesmcardle/Desktop/ECL-Projects/active-projects/veritasSR-DC/50k_data/ps_data_50k.f32 -b 256 --max-blocks 2 --iterations 10 -o trial.zsr
```

Choose block size 256, 512, 1024, or 2048. Every nonconstant block gets an independent
PySR search; exact constant blocks bypass search. Default search budget is 100
iterations per block. Full files may take substantial time; no fixed completion
time or accuracy is guaranteed. PySR can write its own search artifacts in the
working directory. Run from a dedicated output directory if desired.

## Output and accuracy

- `.zsr`: actual binary AST file, not expression text. Compatible with the current
  ex_3 `loadExprArrayAstBinary` version-2 reader.
- Matching `.json`: per-block expressions, source offsets, true block lengths,
  node counts, measured maximum absolute error/RMSE, tolerance status, and settings.
- Each AST uses block-local x=0,1,...,block_size-1. Training uses a normalized
  coordinate, which is explicitly converted into this x coordinate in the AST.
- Coefficients are rounded to float32 before candidates are evaluated. Checks also
  include float32 reconstructed output. Candidates producing nonfinite output are rejected.
- Among candidates meeting the maximum absolute error target, choose the smallest
  serialized tree. Otherwise choose the smallest-error candidate and report the
  target miss. A finite constant baseline remains available and is labeled in JSON.
- Add `--require-tolerance` to refuse to publish outputs if any block misses the target.
  The tolerance in the binary header alone is NOT a guarantee. No residual data is stored.
- The default operator search uses +, -, *, sin, cos, exp. Log is deliberately excluded
  because ex_3 evaluates `log(abs(x)+1e-9)`, which differs from ordinary logarithm.
  Arbitrary powers/unsupported SymPy functions are rejected during export.
- Search is serial with a per-block deterministic seed. Reproducibility across
  PySR/Julia versions is not guaranteed.

## Partial final blocks — important

Version 2 stores a single block size and expression count, but **no original sample
count**. Default `--tail pad` repeats the last value to fill the training block.
Keep the JSON with the .zsr and trim reconstructed output to JSON `sample_count`.
For example, 50,000 samples with block size 1,024 produce 49 expressions and
50,176 decoded samples; only the first 50,000 are original data. Error metrics cover
original samples only. Use `--tail error` to reject incomplete final blocks.
`--max-blocks` intentionally processes just a prefix; JSON records that prefix length.

## Format correspondence

Inspected against ex_3/src/ast_binary_io.cpp and ex_3/include/ast_nodes.h:

- Header: little-endian `<BBIfH` (12 bytes): 0x20 version, zero flags, uint32 block
  size, float32 tolerance, uint16 expression count.
- Each expression: uint16 node count, then preorder AST tokens.
- Variable x: 0x00. Constant: 0x40 followed by little-endian float32.
- Unary: 0x80 | OpKind, then child. Binary: 0xc0 | OpKind, then left/right children.
- OpKind: add=1, subtract=2, multiply=3, divide=4, power=5, sin=6, cos=7, exp=8.
- At most 65,535 expressions and 65,535 nodes per expression.

No format changes to the SR project are required. Legacy version-1-only readers
cannot read this output; use the current version-2 loader.

## Checks performed

```sh
python -m unittest -v test_pysr_to_zsr.py
```

The writer's constant, linear, and sinusoidal ASTs were loaded and evaluated by
ex_3's C++ loader at all four requested block sizes. Header/token tests and
no-clobber output checks and SymPy translation tests also passed. An end-to-end
constant-block run verified partial-tail metadata and strict tolerance handling.
PySR was not installed in the authoring environment, so a live PySR search could
not be run there. Install the requirements and run the two-block trial above.

PySR API reference: https://ai.damtp.cam.ac.uk/pysr/v1.5.9/api.html

#!/usr/bin/env python3
"""Run the LC two-part sweep with lossy compression of the XOR residual.

This runner reuses lc_two_part_compress.py and changes only the residual path:
the XOR residual is compressed with the same swept lossy preprocessor and
error bound as the first stream.  The final reconstruction is measured against
the original input to determine whether it still satisfies that error bound.
"""

from __future__ import annotations

import argparse
import filecmp
import json
from pathlib import Path
import subprocess

import lc_two_part_compress as base

base.RUN_TYPE = "LDLR"


def evaluate_two_part(
    args: argparse.Namespace,
    input_file: Path,
    lc_dir: Path,
    lc_executable: Path,
    run_dir: Path,
    error_bound: float,
    perf_argument: list[str],
) -> dict[str, object]:
    """Evaluate two lossy streams and measure the combined reconstruction."""
    run_dir.mkdir(parents=True, exist_ok=True)
    bound_text = base.error_bound_text(error_bound)
    preprocessor = f"{args.lossy_preprocessor}({bound_text})"
    residual_preprocessor = preprocessor
    filename = input_file.name
    lossy_compressed = run_dir / f"{filename}.lossy.lc"
    lossy_reconstructed = run_dir / f"{filename}.lossy-decoded"
    residual = run_dir / f"{filename}.xor-residual"
    residual_compressed = run_dir / f"{filename}.xor-residual.lossy.lc"
    residual_decoded = run_dir / f"{filename}.xor-residual-lossy-decoded"
    final_reconstruction = run_dir / f"{filename}.final-reconstruction"

    print(f"\n=== Error bound {bound_text}: searching for the first lossy pipeline ===")
    lossy_pipeline = base.search_best_pipeline(
        lc_executable, input_file, preprocessor, args.lossy_pattern, cwd=run_dir
    )

    print(f"\n=== Error bound {bound_text}: building the first lossy programs ===")
    generated = base.generate_sources(lc_dir, preprocessor, lossy_pipeline)
    lossy_compressor, lossy_decompressor = base.preserve_and_build_pair(
        "lossy", *generated, run_dir, args.compiler, lc_dir
    )

    print(f"\n=== Error bound {bound_text}: running the first lossy stream ===")
    base.run(
        [str(lossy_compressor), str(input_file), str(lossy_compressed), *perf_argument],
        cwd=lc_dir,
        description=f"First-stream lossy compression at epsilon {bound_text}",
    )
    base.run(
        [
            str(lossy_decompressor),
            str(lossy_compressed),
            str(lossy_reconstructed),
            *perf_argument,
        ],
        cwd=lc_dir,
        description=f"First-stream lossy decompression at epsilon {bound_text}",
    )
    first_stream_quality = base.lossy_reconstruction_stats(
        input_file, lossy_reconstructed, error_bound, args.lossy_preprocessor
    )

    base.xor_files(input_file, lossy_reconstructed, residual)

    print(f"\n=== Error bound {bound_text}: searching for the lossy residual pipeline ===")
    residual_pipeline = base.search_best_pipeline(
        lc_executable,
        residual,
        residual_preprocessor,
        args.lossless_pattern,
        cwd=run_dir,
    )

    print(f"\n=== Error bound {bound_text}: building the lossy residual programs ===")
    generated = base.generate_sources(
        lc_dir, residual_preprocessor, residual_pipeline
    )
    residual_compressor, residual_decompressor = base.preserve_and_build_pair(
        "residual-lossy", *generated, run_dir, args.compiler, lc_dir
    )

    print(f"\n=== Error bound {bound_text}: running the lossy residual stream ===")
    base.run(
        [
            str(residual_compressor),
            str(residual),
            str(residual_compressed),
            *perf_argument,
        ],
        cwd=lc_dir,
        description=f"Lossy XOR-residual compression at epsilon {bound_text}",
    )
    base.run(
        [
            str(residual_decompressor),
            str(residual_compressed),
            str(residual_decoded),
            *perf_argument,
        ],
        cwd=lc_dir,
        description=f"Lossy XOR-residual decompression at epsilon {bound_text}",
    )

    base.xor_files(lossy_reconstructed, residual_decoded, final_reconstruction)
    final_quality = base.lossy_reconstruction_stats(
        input_file, final_reconstruction, error_bound, args.lossy_preprocessor
    )
    exact = filecmp.cmp(input_file, final_reconstruction, shallow=False)
    error_bound_verified = (
        int(final_quality["points_outside_epsilon"]) == 0
        and int(final_quality["nan_count"]) == 0
        and int(final_quality["inf_count"]) == 0
    )

    original_size = input_file.stat().st_size
    lossy_size = lossy_compressed.stat().st_size
    residual_size = residual_compressed.stat().st_size
    total_size = lossy_size + residual_size
    result: dict[str, object] = {
        "error_bound": error_bound,
        "lossy_preprocessor": preprocessor,
        "lossy_pipeline": lossy_pipeline,
        "lossy_compressed_file": str(lossy_compressed),
        "lossy_compressed_bytes": lossy_size,
        "residual_definition": "original_byte XOR lossy_reconstructed_byte",
        "residual_preprocessor": residual_preprocessor,
        "lossless_residual_pipeline": residual_pipeline,
        "lossy_residual_pipeline": residual_pipeline,
        "residual_compressed_file": str(residual_compressed),
        "residual_compressed_bytes": residual_size,
        "combined_compressed_bytes": total_size,
        "compressed_percent_of_original": 100.0 * total_size / original_size,
        "compression_ratio_original_over_compressed": original_size / total_size,
        "first_stream_quality": first_stream_quality,
        **final_quality,
        "error_bound_verified": error_bound_verified,
        "exact_reconstruction_verified": exact,
    }
    run_report = {
        "input": str(input_file),
        "original_bytes": original_size,
        "quality_statistics_apply_to": "final two-stream reconstruction",
        **result,
    }
    (run_dir / f"report_{base.RUN_TYPE}.json").write_text(
        json.dumps(run_report, indent=2) + "\n", encoding="utf-8"
    )

    status = "PASSED" if error_bound_verified else "FAILED"
    print(
        f"Final epsilon check: {status} "
        f"({int(final_quality['points_outside_epsilon']):,} points outside epsilon)"
    )
    return result


def sweep_csv_rows(
    input_file: Path,
    original_size: int,
    result: dict[str, object],
    lossless_baselines: dict[str, dict[str, object]],
) -> list[dict[str, object]]:
    rows = base.original_sweep_csv_rows(
        input_file, original_size, result, lossless_baselines
    )
    for row in rows:
        row["residual_preprocessor"] = ""
        row["error_bound_verified"] = row["exact_reconstruction_verified"]

    two_part = rows[0]
    two_part["cmd"] = "lc_two_part_lossy_residual"
    two_part["mode"] = "lossy stream plus lossy XOR residual"
    two_part["residual_preprocessor"] = result["residual_preprocessor"]
    two_part["error_bound_verified"] = result["error_bound_verified"]
    return rows


def build_parser() -> argparse.ArgumentParser:
    parser = base.original_build_parser()
    parser.description = (
        "Sweep ten logarithmic error bounds from 0.01 through 10 for an LC "
        "lossy stream plus an LC lossy-compressed XOR-residual stream, then "
        "test the final reconstruction against each error bound."
    )
    return parser


base.original_sweep_csv_rows = base.sweep_csv_rows
base.original_build_parser = base.build_parser
base.evaluate_two_part = evaluate_two_part
base.sweep_csv_rows = sweep_csv_rows
base.build_parser = build_parser
base.SWEEP_CSV_FIELDS = [
    *base.SWEEP_CSV_FIELDS,
    "residual_preprocessor",
    "error_bound_verified",
]


if __name__ == "__main__":
    try:
        raise SystemExit(base.main())
    except (ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"\nERROR: {error}", file=base.sys.stderr)
        if base.COMMAND_LOG_FILE is not None:
            print(f"Command output: {base.COMMAND_LOG_FILE}", file=base.sys.stderr)
        raise SystemExit(1)

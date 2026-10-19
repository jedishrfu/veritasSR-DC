#!/usr/bin/env python3
"""Build and evaluate a two-part LC compression error-bound sweep.

The first stream is produced by a generated lossy LC compressor.  The second
stream losslessly stores the bytewise XOR between the original data and the
lossy reconstruction.  Together, the two streams can reproduce the original
file exactly.  The lossy error bound is swept logarithmically over ten values
from 0.01 through 10.  Lossless baselines are run separately by lc_lossless_compress.py.
Child-process output is
written to a Markdown command log in the report directory.

This script is intended for Dr. Martin Burtscher's LC Framework:
https://github.com/burtscher/LC-framework
"""

from __future__ import annotations

import argparse
import csv
from datetime import datetime
import filecmp
import json
import math
import os
from pathlib import Path
import shlex
import shutil
import struct
import subprocess
import sys


DEFAULT_PATTERN = ".+ .+ R.+|C.+|H.+"
DEFAULT_LOSSY_PREPROCESSOR = "QUANT_IABS_0_f32"
ERROR_BOUND_MIN = 0.01
ERROR_BOUND_MAX = 10.0
ERROR_BOUND_COUNT = 10
GENERATED_COMPRESSOR = "compressor-standalone.cpp"
GENERATED_DECOMPRESSOR = "decompressor-standalone.cpp"
RUN_TYPE = "LDLLR"
COMMAND_LOG_FILE: Path | None = None


def command_text(command: list[str]) -> str:
    return shlex.join(str(part) for part in command)


def initialize_command_log(log_file: Path, input_file: Path) -> None:
    global COMMAND_LOG_FILE
    COMMAND_LOG_FILE = log_file
    log_file.write_text(
        "# Compression command log\n\n"
        f"- Input: `{input_file}`\n"
        f"- Started: `{datetime.now().isoformat(timespec='seconds')}`\n\n",
        encoding="utf-8",
    )


def command_log() -> Path:
    if COMMAND_LOG_FILE is None:
        raise RuntimeError("The command log has not been initialized")
    return COMMAND_LOG_FILE


def write_command_log_entry(
    command: list[str],
    cwd: Path,
    output: str,
    return_code: int,
    description: str,
) -> None:
    with command_log().open("a", encoding="utf-8") as log:
        log.write(f"## {description}\n\n")
        log.write(f"Working directory: `{cwd}`\n\n")
        log.write("```console\n")
        log.write(f"$ {command_text(command)}\n")
        if output:
            log.write(output)
            if not output.endswith("\n"):
                log.write("\n")
        log.write("```\n\n")
        log.write(f"Exit status: `{return_code}`\n\n")
        log.flush()
        os.fsync(log.fileno())


def run(command: list[str], *, cwd: Path, description: str | None = None) -> None:
    process = subprocess.run(
        command,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    write_command_log_entry(
        command,
        cwd,
        process.stdout or "",
        process.returncode,
        description or Path(command[0]).name,
    )
    if process.returncode != 0:
        raise subprocess.CalledProcessError(process.returncode, command)


def search_best_pipeline(
    lc_executable: Path,
    input_file: Path,
    preprocessor: str,
    pattern: str,
    *,
    cwd: Path,
) -> str:
    """Run LC in CR mode, log its output, and return its best pipeline."""
    command = [
        str(lc_executable),
        str(input_file),
        "CR",
        preprocessor,
        pattern,
    ]
    process = subprocess.Popen(
        command,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )
    assert process.stdout is not None

    last_pipeline: str | None = None
    overall_pipeline: str | None = None
    waiting_for_overall_pipeline = False

    captured_output: list[str] = []
    for line in process.stdout:
        captured_output.append(line)
        stripped = line.strip()
        if stripped == "Overall best":
            waiting_for_overall_pipeline = True
        elif stripped.startswith("pipeline:"):
            pipeline = stripped.partition(":")[2].strip()
            if pipeline:
                last_pipeline = pipeline
                if waiting_for_overall_pipeline:
                    overall_pipeline = pipeline
                    waiting_for_overall_pipeline = False

    return_code = process.wait()
    write_command_log_entry(
        command,
        cwd,
        "".join(captured_output),
        return_code,
        "LC pipeline search",
    )
    if return_code != 0:
        raise subprocess.CalledProcessError(return_code, command)

    pipeline = overall_pipeline or last_pipeline
    if not pipeline:
        raise RuntimeError(
            "Could not locate a 'pipeline:' line in LC's output. "
            "Check that this script matches your LC Framework version."
        )
    print(f"Selected pipeline: {pipeline}", flush=True)
    return pipeline


def generate_sources(
    lc_dir: Path,
    preprocessor: str,
    pipeline: str,
) -> tuple[Path, Path]:
    generator = lc_dir / "generate_standalone_CPU_compressor_decompressor.py"
    compressor = lc_dir / GENERATED_COMPRESSOR
    decompressor = lc_dir / GENERATED_DECOMPRESSOR

    components = pipeline.split()
    standalone_components = [component for component in components if component != "NUL"]
    if len(standalone_components) != len(components):
        print("Removing identity component NUL from the standalone pipeline")
    standalone_pipeline = " ".join(standalone_components)
    if not standalone_pipeline:
        raise RuntimeError(
            f"LC selected a pipeline containing only identity components: {pipeline}"
        )

    # The LC generator can print an error and still exit successfully.  Remove
    # prior outputs and inspect its text so invalid output is never compiled.
    compressor.unlink(missing_ok=True)
    decompressor.unlink(missing_ok=True)
    command = [sys.executable, str(generator), preprocessor, standalone_pipeline]
    process = subprocess.run(
        command,
        cwd=lc_dir,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    write_command_log_entry(
        command,
        lc_dir,
        process.stdout or "",
        process.returncode,
        "Generate LC standalone sources",
    )
    if process.returncode != 0:
        raise subprocess.CalledProcessError(process.returncode, command, output=process.stdout)
    generator_error = process.stdout and (
        "does not exist" in process.stdout or "does nor exist" in process.stdout
    )
    if generator_error:
        raise RuntimeError(
            "LC standalone generation failed for pipeline "
            f"{standalone_pipeline!r}; see {command_log()}"
        )
    if not compressor.is_file() or not decompressor.is_file():
        raise RuntimeError("LC did not generate the expected standalone sources")
    return compressor, decompressor


def compile_program(
    compiler: str,
    source: Path,
    executable: Path,
    *,
    lc_dir: Path,
) -> None:
    command = [
        compiler,
        "-O3",
        "-march=native",
        "-fopenmp",
        "-ffp-contract=off",
        f"-I{lc_dir}",
        "-std=c++17",
        "-o",
        str(executable),
        str(source),
    ]
    run(command, cwd=lc_dir)


def preserve_and_build_pair(
    label: str,
    generated_compressor: Path,
    generated_decompressor: Path,
    output_dir: Path,
    compiler: str,
    lc_dir: Path,
) -> tuple[Path, Path]:
    compressor_source = output_dir / f"{label}-compressor.cpp"
    decompressor_source = output_dir / f"{label}-decompressor.cpp"
    compressor_executable = output_dir / f"{label}-compress"
    decompressor_executable = output_dir / f"{label}-decompress"

    shutil.copy2(generated_compressor, compressor_source)
    shutil.copy2(generated_decompressor, decompressor_source)
    compile_program(compiler, compressor_source, compressor_executable, lc_dir=lc_dir)
    compile_program(compiler, decompressor_source, decompressor_executable, lc_dir=lc_dir)
    return compressor_executable, decompressor_executable


def xor_files(left: Path, right: Path, output: Path, chunk_size: int = 1024 * 1024) -> None:
    if left.stat().st_size != right.stat().st_size:
        raise RuntimeError(
            "The lossy reconstruction has a different size from the original: "
            f"{left.stat().st_size} versus {right.stat().st_size} bytes"
        )

    with left.open("rb") as left_file, right.open("rb") as right_file, output.open("wb") as out:
        while True:
            left_block = left_file.read(chunk_size)
            right_block = right_file.read(chunk_size)
            if not left_block:
                break
            if len(left_block) != len(right_block):
                raise RuntimeError("Input sizes changed while generating XOR residuals")
            out.write(bytes(a ^ b for a, b in zip(left_block, right_block)))


def lossy_reconstruction_stats(
    original_file: Path,
    reconstructed_file: Path,
    error_bound: float,
    lossy_preprocessor: str,
) -> dict[str, object]:
    """Calculate error statistics for a lossy floating-point reconstruction.

    Error aggregates and epsilon counts use pairs in which both values are
    finite.  NaN and infinity counts describe the reconstructed stream.
    """
    preprocessor_name = lossy_preprocessor.lower()
    if "_f32" in preprocessor_name:
        value_format = "=f"
        value_size = 4
    elif "_f64" in preprocessor_name:
        value_format = "=d"
        value_size = 8
    else:
        raise ValueError(
            "Cannot calculate floating-point error statistics because the lossy "
            f"preprocessor type is unknown: {lossy_preprocessor}"
        )

    original_bytes = original_file.read_bytes()
    reconstructed_bytes = reconstructed_file.read_bytes()
    if len(original_bytes) != len(reconstructed_bytes):
        raise RuntimeError(
            "The lossy reconstruction has a different size from the original: "
            f"{len(original_bytes)} versus {len(reconstructed_bytes)} bytes"
        )
    if len(original_bytes) % value_size:
        raise RuntimeError(
            f"Input size {len(original_bytes)} is not a multiple of {value_size} bytes"
        )

    original_values = struct.iter_unpack(value_format, original_bytes)
    reconstructed_values = struct.iter_unpack(value_format, reconstructed_bytes)
    finite_count = 0
    nan_count = 0
    inf_count = 0
    points_within_epsilon = 0
    points_outside_epsilon = 0
    sum_absolute_error = 0.0
    sum_squared_error = 0.0
    max_absolute_error = 0.0
    peak_absolute_original = 0.0

    for (original,), (reconstructed,) in zip(original_values, reconstructed_values):
        if math.isnan(reconstructed):
            nan_count += 1
        elif math.isinf(reconstructed):
            inf_count += 1

        if not math.isfinite(original) or not math.isfinite(reconstructed):
            continue

        absolute_error = abs(original - reconstructed)
        finite_count += 1
        sum_absolute_error += absolute_error
        sum_squared_error += absolute_error * absolute_error
        max_absolute_error = max(max_absolute_error, absolute_error)
        peak_absolute_original = max(peak_absolute_original, abs(original))
        if absolute_error <= error_bound:
            points_within_epsilon += 1
        else:
            points_outside_epsilon += 1

    if finite_count == 0:
        mse = None
        rmse = None
        mean_absolute_error = None
        psnr_db = None
    else:
        mse = sum_squared_error / finite_count
        rmse = math.sqrt(mse)
        mean_absolute_error = sum_absolute_error / finite_count
        if rmse > 0.0 and peak_absolute_original > 0.0:
            psnr_db = 20.0 * math.log10(peak_absolute_original / rmse)
        elif rmse == 0.0 and peak_absolute_original > 0.0:
            psnr_db = math.inf
        else:
            psnr_db = None

    return {
        "psnr_db": psnr_db,
        "mse": mse,
        "rmse": rmse,
        "nan_count": nan_count,
        "inf_count": inf_count,
        "points_within_epsilon": points_within_epsilon,
        "points_outside_epsilon": points_outside_epsilon,
        "max_absolute_error": max_absolute_error if finite_count else None,
        "mean_absolute_error": mean_absolute_error,
    }


def logarithmic_values(lower: float, upper: float, count: int) -> list[float]:
    """Return count values, including both endpoints, evenly spaced in log10."""
    if lower <= 0 or upper <= lower or count < 2:
        raise ValueError("Invalid logarithmic sweep configuration")
    start = math.log10(lower)
    stop = math.log10(upper)
    return [10 ** (start + index * (stop - start) / (count - 1)) for index in range(count)]


def error_bound_text(error_bound: float) -> str:
    return f"{error_bound:.12g}"


def statistic_text(value: object, format_spec: str = ".6g") -> str:
    return "n/a" if value is None else format(float(value), format_spec)


def error_bound_directory(index: int, error_bound: float) -> str:
    label = error_bound_text(error_bound).replace(".", "p").replace("+", "")
    return f"{index + 1:02d}-error-bound-{label}"


def validate_arguments(args: argparse.Namespace) -> tuple[Path, Path, Path]:
    if args.epsilon_points < 2:
        raise ValueError("--epsilon-points must be at least 2")
    input_file = Path(args.input).expanduser().resolve()
    lc_dir = Path(args.lc_dir).expanduser().resolve()
    if args.output_dir:
        output_dir = Path(args.output_dir).expanduser().resolve()
    else:
        timestamp = datetime.now().strftime("%Y%m%d-%H%M")
        output_dir = (
            Path("testrpts") / f"{input_file.name}_{RUN_TYPE}_{timestamp}"
        ).resolve()

    if not input_file.is_file():
        raise ValueError(f"Input file does not exist: {input_file}")
    if input_file.stat().st_size == 0:
        raise ValueError("The input file is empty")
    if not lc_dir.is_dir():
        raise ValueError(f"LC directory does not exist: {lc_dir}")
    if not (lc_dir / "lc").is_file():
        raise ValueError(f"The built LC executable was not found at {lc_dir / 'lc'}")
    if not (lc_dir / "generate_standalone_CPU_compressor_decompressor.py").is_file():
        raise ValueError("The LC CPU standalone generator was not found")

    if "(" in args.lossy_preprocessor or ")" in args.lossy_preprocessor:
        raise ValueError(
            "--lossy-preprocessor must be a preprocessor name without parameters; "
            "the script supplies each swept error bound"
        )

    preprocessor_name = args.lossy_preprocessor.lower()
    word_size = 4 if "_f32" in preprocessor_name else 8 if "_f64" in preprocessor_name else None
    if word_size and input_file.stat().st_size % word_size:
        raise ValueError(
            f"Input size must be a multiple of {word_size} bytes for "
            f"{args.lossy_preprocessor}"
        )
    output_dir.mkdir(parents=True, exist_ok=True)
    return input_file, lc_dir, output_dir


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Sweep logarithmic error bounds from 0.01 through 10 for a "
            "lossy LC stream plus a losslessly compressed XOR-residual stream."
        )
    )
    parser.add_argument("input", help="Raw binary floating-point test data")
    parser.add_argument("--epsilon-points", type=int, default=ERROR_BOUND_COUNT,
                        help="Number of logarithmic epsilon sweep points (default: %(default)s)")
    parser.add_argument(
        "--lc-dir",
        default=".",
        help="LC-framework directory containing the built ./lc command (default: .)",
    )
    parser.add_argument(
        "--output-dir",
        default=None,
        help=(
            "Directory for generated programs and data "
            f"(default: testrpts/INPUT_FILE_NAME_{RUN_TYPE}_YYYYMMDD-HHMM)"
        ),
    )
    parser.add_argument(
        "--lossy-preprocessor",
        default=DEFAULT_LOSSY_PREPROCESSOR,
        help=(
            "LC lossy preprocessor name; the swept error bound is appended automatically "
            f"(default: {DEFAULT_LOSSY_PREPROCESSOR})"
        ),
    )
    parser.add_argument(
        "--lossy-pattern",
        default=DEFAULT_PATTERN,
        help=f"LC component search pattern for lossy data (default: {DEFAULT_PATTERN!r})",
    )
    parser.add_argument(
        "--lossless-pattern",
        default=DEFAULT_PATTERN,
        help=f"LC component search pattern for XOR residuals (default: {DEFAULT_PATTERN!r})",
    )
    parser.add_argument(
        "--compiler",
        default=os.environ.get("CXX", "g++-16"),
        help="C++ compiler (default: CXX environment variable or g++)",
    )
    parser.add_argument(
        "--performance",
        action="store_true",
        help="Ask generated LC programs to print throughput measurements",
    )
    parser.add_argument(
        "--lossless-csv",
        default=str(Path(__file__).resolve().parent / "testrpts/lossless_results.csv"),
        help="Common lossless CSV to append combined two-part results to",
    )
    return parser


def evaluate_two_part(
    args: argparse.Namespace,
    input_file: Path,
    lc_dir: Path,
    lc_executable: Path,
    run_dir: Path,
    error_bound: float,
    perf_argument: list[str],
) -> dict[str, object]:
    run_dir.mkdir(parents=True, exist_ok=True)
    bound_text = error_bound_text(error_bound)
    preprocessor = f"{args.lossy_preprocessor}({bound_text})"
    base = input_file.name
    lossy_compressed = run_dir / f"{base}.lossy.lc"
    lossy_reconstructed = run_dir / f"{base}.lossy-decoded"
    residual = run_dir / f"{base}.xor-residual"
    residual_compressed = run_dir / f"{base}.xor-residual.lc"
    residual_decoded = run_dir / f"{base}.xor-residual-decoded"
    exact_reconstruction = run_dir / f"{base}.exact-reconstruction"

    print(f"\n=== Error bound {bound_text}: searching for the lossy pipeline ===")
    lossy_pipeline = search_best_pipeline(
        lc_executable, input_file, preprocessor, args.lossy_pattern, cwd=run_dir
    )

    print(f"\n=== Error bound {bound_text}: building the lossy programs ===")
    generated = generate_sources(lc_dir, preprocessor, lossy_pipeline)
    lossy_compressor, lossy_decompressor = preserve_and_build_pair(
        "lossy", *generated, run_dir, args.compiler, lc_dir
    )

    print(f"\n=== Error bound {bound_text}: running the lossy stream ===")
    run(
        [str(lossy_compressor), str(input_file), str(lossy_compressed), *perf_argument],
        cwd=lc_dir,
    )
    run(
        [str(lossy_decompressor), str(lossy_compressed), str(lossy_reconstructed), *perf_argument],
        cwd=lc_dir,
    )

    quality_stats = lossy_reconstruction_stats(
        input_file,
        lossy_reconstructed,
        error_bound,
        args.lossy_preprocessor,
    )

    xor_files(input_file, lossy_reconstructed, residual)

    print(f"\n=== Error bound {bound_text}: searching for the residual pipeline ===")
    residual_pipeline = search_best_pipeline(
        lc_executable, residual, "", args.lossless_pattern, cwd=run_dir
    )

    print(f"\n=== Error bound {bound_text}: building the residual programs ===")
    generated = generate_sources(lc_dir, "", residual_pipeline)
    residual_compressor, residual_decompressor = preserve_and_build_pair(
        "residual", *generated, run_dir, args.compiler, lc_dir
    )

    print(f"\n=== Error bound {bound_text}: compressing and verifying the residual ===")
    run(
        [str(residual_compressor), str(residual), str(residual_compressed), *perf_argument],
        cwd=lc_dir,
    )
    run(
        [str(residual_decompressor), str(residual_compressed), str(residual_decoded), *perf_argument],
        cwd=lc_dir,
    )
    xor_files(lossy_reconstructed, residual_decoded, exact_reconstruction)
    exact = filecmp.cmp(input_file, exact_reconstruction, shallow=False)
    if not exact:
        raise RuntimeError(
            f"Two-part reconstruction for error bound {bound_text} did not match the original"
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
        "lossless_residual_pipeline": residual_pipeline,
        "residual_compressed_file": str(residual_compressed),
        "residual_compressed_bytes": residual_size,
        "combined_compressed_bytes": total_size,
        "compressed_percent_of_original": 100.0 * total_size / original_size,
        "compression_ratio_original_over_compressed": original_size / total_size,
        **quality_stats,
        "exact_reconstruction_verified": exact,
    }
    run_report = {
        "input": str(input_file),
        "original_bytes": original_size,
        **result,
    }
    (run_dir / f"report_{RUN_TYPE}.json").write_text(
        json.dumps(run_report, indent=2) + "\n", encoding="utf-8"
    )
    return result


SWEEP_CSV_FIELDS = [
    "input",
    "original_bytes",
    "eps",
    "cmd",
    "mode",
    "preprocessor",
    "pipeline",
    "residual_pipeline",
    "compressed_file",
    "lossy_compressed_file",
    "residual_compressed_file",
    "lossy_compressed_bytes",
    "residual_compressed_bytes",
    "compressed_bytes",
    "compressed_percent_of_original",
    "CR",
    "psnr_db",
    "mse",
    "rmse",
    "nan_count",
    "inf_count",
    "points_within_epsilon",
    "points_outside_epsilon",
    "MaxAbsErr",
    "mae",
    "exact_reconstruction_verified",
]


def csv_statistic_text(value: object) -> str:
    """Format a floating-point CSV statistic with three significant digits."""
    if value is None or value == "":
        return ""
    return format(float(value), ".3g")


def sweep_csv_rows(
    input_file: Path,
    original_size: int,
    result: dict[str, object],
    lossless_baselines: dict[str, dict[str, object]] | None = None,
) -> list[dict[str, object]]:
    bound = csv_statistic_text(result["error_bound"])
    rows = [{
        "input": input_file.name,
        "original_bytes": original_size,
        "eps": bound,
        "cmd": "lc_two_part",
        "mode": "lossy stream plus lossless XOR residual",
        "preprocessor": result["lossy_preprocessor"],
        "pipeline": result["lossy_pipeline"],
        "residual_pipeline": result["lossless_residual_pipeline"],
        "compressed_file": "",
        "lossy_compressed_file": Path(str(result["lossy_compressed_file"])).name,
        "residual_compressed_file": Path(str(result["residual_compressed_file"])).name,
        "lossy_compressed_bytes": result["lossy_compressed_bytes"],
        "residual_compressed_bytes": result["residual_compressed_bytes"],
        "compressed_bytes": result["combined_compressed_bytes"],
        "compressed_percent_of_original": csv_statistic_text(
            result["compressed_percent_of_original"]
        ),
        "CR": csv_statistic_text(
            result["compression_ratio_original_over_compressed"]
        ),
        "psnr_db": csv_statistic_text(result["psnr_db"]),
        "mse": csv_statistic_text(result["mse"]),
        "rmse": csv_statistic_text(result["rmse"]),
        "nan_count": result["nan_count"],
        "inf_count": result["inf_count"],
        "points_within_epsilon": result["points_within_epsilon"],
        "points_outside_epsilon": result["points_outside_epsilon"],
        "MaxAbsErr": csv_statistic_text(result["max_absolute_error"]),
        "mae": csv_statistic_text(result["mean_absolute_error"]),
        "exact_reconstruction_verified": result["exact_reconstruction_verified"],
    }]

    return rows


def append_combined_lossless_row(
    csv_file: Path, input_file: Path, result: dict[str, object]
) -> None:
    """Append final storage statistics; leave unmeasured timing fields blank."""
    from lc_lossless_compress import FIELDS, append_row

    row = dict.fromkeys(FIELDS, "")
    row.update({
        "file": input_file.name,
        "command": "LC lossy + lossless XOR residual",
        "command_args": command_text([
            "--epsilon", error_bound_text(float(result["error_bound"])),
            "--preprocessor", str(result["lossy_preprocessor"]),
            "--lossy-pipeline", str(result["lossy_pipeline"]),
            "--residual-pipeline", str(result["lossless_residual_pipeline"]),
        ]),
        "original_bytes": input_file.stat().st_size,
        "compressed_bytes": result["combined_compressed_bytes"],
        "compression_ratio": result["compression_ratio_original_over_compressed"],
        "exact_reconstruction": result["exact_reconstruction_verified"],
    })
    append_row(csv_file, row)


def main() -> int:
    args = build_parser().parse_args()
    input_file, lc_dir, output_dir = validate_arguments(args)
    lc_executable = (lc_dir / "lc").resolve()
    perf_argument = ["y"] if args.performance else []
    original_size = input_file.stat().st_size
    error_bounds = logarithmic_values(ERROR_BOUND_MIN, ERROR_BOUND_MAX, args.epsilon_points)
    command_log_file = output_dir / f"command-output_{RUN_TYPE}.md"
    initialize_command_log(command_log_file, input_file)

    csv_file = output_dir / f"sweep_results_{RUN_TYPE}.csv"
    common_csv = Path(args.lossless_csv).expanduser().resolve()
    sweep_results: list[dict[str, object]] = []
    with csv_file.open("w", newline="", encoding="utf-8") as csv_output:
        csv_writer = csv.DictWriter(csv_output, fieldnames=SWEEP_CSV_FIELDS)
        csv_writer.writeheader()
        csv_output.flush()
        os.fsync(csv_output.fileno())

        for index, error_bound in enumerate(error_bounds):
            run_dir = output_dir / error_bound_directory(index, error_bound)
            result = evaluate_two_part(
                args,
                input_file,
                lc_dir,
                lc_executable,
                run_dir,
                error_bound,
                perf_argument,
            )
            sweep_results.append(result)
            for row in sweep_csv_rows(
                input_file, original_size, result
            ):
                csv_writer.writerow(row)
                csv_output.flush()
                os.fsync(csv_output.fileno())
            if RUN_TYPE == "LDLLR":
                append_combined_lossless_row(common_csv, input_file, result)
            print(f"Saved and flushed the sweep row to {csv_file}")

    best_result = min(
        sweep_results,
        key=lambda result: int(result["combined_compressed_bytes"]),
    )
    report = {
        "input": str(input_file),
        "run_type": RUN_TYPE,
        "original_bytes": original_size,
        "lossy_preprocessor_name": args.lossy_preprocessor,
        "error_bound_sweep": {
            "scale": "logarithmic",
            "minimum": ERROR_BOUND_MIN,
            "maximum": ERROR_BOUND_MAX,
            "count": args.epsilon_points,
            "values": error_bounds,
        },
        "sweep_results": sweep_results,
        "best_two_part_result": best_result,
        "sweep_csv": str(csv_file),
        "lossless_csv": str(common_csv) if RUN_TYPE == "LDLLR" else None,
        "command_log": str(command_log_file),
    }
    report_file = output_dir / f"report_{RUN_TYPE}.json"
    report_file.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    print("\n=== Error-bound sweep results ===")
    print("Error bound       Lossy bytes   Residual bytes   Combined bytes   Percent   Ratio")
    for result in sweep_results:
        print(
            f"{error_bound_text(float(result['error_bound'])):>11}   "
            f"{int(result['lossy_compressed_bytes']):>11,}   "
            f"{int(result['residual_compressed_bytes']):>14,}   "
            f"{int(result['combined_compressed_bytes']):>14,}   "
            f"{float(result['compressed_percent_of_original']):>6.2f}%   "
            f"{float(result['compression_ratio_original_over_compressed']):>7.4f}x"
        )
    print("\n=== Lossy reconstruction quality ===")
    print(
        "Epsilon       CR       PSNR(dB)       MSE       RMSE        MAE      "
        "MaxAE   NaN   Inf      Within     Outside"
    )
    for result in sweep_results:
        print(
            f"{error_bound_text(float(result['error_bound'])):>7}   "
            f"{float(result['compression_ratio_original_over_compressed']):>7.4f}x   "
            f"{statistic_text(result['psnr_db'], '.4f'):>9}   "
            f"{statistic_text(result['mse']):>9}   "
            f"{statistic_text(result['rmse']):>9}   "
            f"{statistic_text(result['mean_absolute_error']):>9}   "
            f"{statistic_text(result['max_absolute_error']):>9}   "
            f"{int(result['nan_count']):>3}   "
            f"{int(result['inf_count']):>3}   "
            f"{int(result['points_within_epsilon']):>9,}   "
            f"{int(result['points_outside_epsilon']):>9,}"
        )
    print(
        "\nBest two-part error bound: "
        f"{error_bound_text(float(best_result['error_bound']))} "
        f"({int(best_result['combined_compressed_bytes']):,} bytes)"
    )
    print(f"CSV:                 {csv_file}")
    print(f"Report:              {report_file}")
    print(f"Command log:         {command_log_file}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"\nERROR: {error}", file=sys.stderr)
        if COMMAND_LOG_FILE is not None:
            print(f"Command output: {COMMAND_LOG_FILE}", file=sys.stderr)
        raise SystemExit(1)

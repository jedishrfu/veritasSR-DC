#!/usr/bin/env python3
"""
compress_all_f32.py

Loop through all *.f32 files in the current directory and run:
  1) pwl  <input> <epserance>
  2) sz3  -z -i <input> -o <output.sz>  -t f32 -a <eps> -1 N --print-stats
  3) zfp  -f -i <input> -o <output.zfp> -t f32 -a <eps> -1 N --stats

Usage:
  python3 run-all-f32-compression-tests.py -i data.f32 \
      -tsweep 0.01:1:count=10 \
      -g 12 -s 4 -bsweep 250:2000:count=4 \
      [-n 1000] [--force]

Assumptions:
- Each .f32 file is raw little-endian 32-bit floats with NO header.
- Tools available on PATH: `pwl`, `sz3`, and `zfp`.
- For pwl, CLI is `pwl <input> <epsilon>` (as specified).
"""

import argparse
import numpy as np

from datetime import datetime

import os
import glob
import shutil
import subprocess
import tempfile
import time

from pathlib import Path

def which_or_fail(cmd):

    path = shutil.which(cmd)

    if not path:
        raise FileNotFoundError(
            f"- [WARN] Required tool '{cmd}' not found on PATH. Please install it or add it to PATH."
        )

    return path

def human_bytes(n):
    for unit in ("B","KB","MB","GB","TB"):
        if n < 1024 or unit == "TB":
            return f"{n:.2f} {unit}"
        n /= 1024

def parse_sweep(spec, scale, integer, name):
    """Parse start:stop:step or start:stop:count=N with inclusive bounds."""
    parts = spec.split(":")
    if len(parts) != 3 or not all(parts):
        raise ValueError(
            f"{name} must use start:stop:step or start:stop:count=N"
        )

    try:
        start = float(parts[0])
        stop = float(parts[1])
    except ValueError as exc:
        raise ValueError(f"{name} start and stop must be numbers") from exc

    if start > stop:
        raise ValueError(f"{name} start must be <= stop")
    if scale == "geometric" and (start <= 0 or stop <= 0):
        raise ValueError(f"{name} geometric bounds must be > 0")

    resolution = parts[2]
    if resolution.startswith("count="):
        try:
            count = int(resolution.removeprefix("count="))
        except ValueError as exc:
            raise ValueError(f"{name} count must be an integer") from exc
        if count <= 0:
            raise ValueError(f"{name} count must be > 0")
        values = (
            np.geomspace(start, stop, num=count)
            if scale == "geometric"
            else np.linspace(start, stop, num=count)
        ).tolist()
    else:
        try:
            step = float(resolution)
        except ValueError as exc:
            raise ValueError(
                f"{name} step must be numeric or written as count=N"
            ) from exc
        if scale == "linear":
            if step <= 0:
                raise ValueError(f"{name} linear step must be > 0")
            values = np.arange(start, stop + step * 0.5, step).tolist()
            values = [value for value in values if value <= stop + 1e-12]
        else:
            if step <= 1:
                raise ValueError(f"{name} geometric step (ratio) must be > 1")
            values = []
            value = start
            while value <= stop * (1.0 + 1e-12):
                values.append(value)
                value *= step
                if len(values) > 1_000_000:
                    raise ValueError(f"{name} produced too many values")

    if integer:
        rounded = [int(round(value)) for value in values]
        if any(abs(value - rounded_value) > 1e-9
               for value, rounded_value in zip(values, rounded)):
            raise ValueError(f"{name} must produce integer values")
        values = rounded

    if not values:
        raise ValueError(f"{name} produced no values")
    if len(set(values)) != len(values):
        raise ValueError(f"{name} produced duplicate values")
    return values

def run(cmd, log_path, checkpoint_callback=None):

    start = time.time()
    with tempfile.TemporaryFile(mode="w+", encoding="utf-8") as stdout_file, \
         tempfile.TemporaryFile(mode="w+", encoding="utf-8") as stderr_file:
        proc = subprocess.Popen(
            cmd, stdout=stdout_file, stderr=stderr_file, text=True
        )
        while proc.poll() is None:
            if checkpoint_callback is not None:
                checkpoint_callback()
            time.sleep(0.25)

        stdout_file.seek(0)
        stderr_file.seek(0)
        stdout = stdout_file.read()
        stderr = stderr_file.read()
    elapsed = time.time() - start

    cmd_name = ' '.join(cmd)

    # Write combined log
    with open(log_path, "w", encoding="utf-8") as f:

        print(f"- $ {' '.join(cmd)}\n\n")
        print(f"- Exit code: {proc.returncode}\n\n")
        print(f"- Elapsed: {elapsed:.6f} s\n\n")

        f.write(f"- $ {' '.join(cmd)}\n\n")
        f.write(f"- Exit code: {proc.returncode}\n\n")
        f.write(f"- Elapsed: {elapsed:.6f} s\n\n")

        if stdout:
            print("<details>")
            print(f"<summary>stdout from command: <code>{cmd[0]}</code></summary>\n")
            print(stdout)
            print("\n</details>\n")

            f.write("<details>\n")
            f.write(
                f"<summary>stdout from command: "
                f"<code>{cmd[0]}</code></summary>\n\n"
            )
            f.write(stdout)
            if not stdout.endswith("\n"):
                f.write("\n")
            f.write("\n</details>\n")

        if stderr:
            print("<details>")
            print(f"<summary>stderr from command: <code>{cmd[0]}</code></summary>\n")
            print(stderr)
            print("\n</details>\n")

            f.write("<details>\n")
            f.write(
                f"<summary>stderr from command: "
                f"<code>{cmd[0]}</code></summary>\n\n"
            )
            f.write(stderr)
            if not stderr.endswith("\n"):
                f.write("\n")
            f.write("\n</details>\n")

    return proc.returncode, elapsed

def errorMetrics(
    fname,
    cmd_name,
    elapsed_time,
    eps,
    raw_path,
    zip_path,
    dec_path,
    results_path,
    num_blocks="",
    num_generations="",
    block_size="",
    count=-1
):
    print(f"\n\nError metrics for command: {cmd_name}\n\n")

    try:
        raw = np.fromfile(str(raw_path), dtype=np.float32, count=count)
        dec = np.fromfile(str(dec_path), dtype=np.float32, count=count)

        if dec.size != raw.size:
            print(f"- [WARN] decompressed size {dec.size} != raw size {raw.size}; skipping metrics.")
            return False

        raw_nan_count = int(np.isnan(raw).sum())
        raw_inf_count = int(np.isinf(raw).sum())
        dec_nan_count = int(np.isnan(dec).sum())
        dec_inf_count = int(np.isinf(dec).sum())

        valid = np.isfinite(raw) & np.isfinite(dec)
        valid_count = int(valid.sum())

        if valid_count == 0:
            max_abs_err = float("nan")
            mae = float("nan")
            rmse = float("nan")
            psnr = float("nan")
            l_inf = float("nan")
        else:
            raw_valid = raw[valid].astype(np.float64)
            dec_valid = dec[valid].astype(np.float64)
            err = dec_valid - raw_valid
            abs_err = np.abs(err)

            max_abs_err = float(abs_err.max())
            mae = float(abs_err.mean())
            mse = float(np.mean(err * err))
            rmse = float(np.sqrt(mse))
            l_inf = max_abs_err

            finite_raw = raw[np.isfinite(raw)].astype(np.float64)
            data_range = (
                float(finite_raw.max() - finite_raw.min())
                if finite_raw.size
                else float("nan")
            )

            if rmse == 0.0:
                psnr = float("inf")
            elif not np.isfinite(data_range) or data_range <= 0.0:
                psnr = float("nan")
            else:
                psnr = float(20.0 * np.log10(data_range / rmse))

        try:
            cr = raw.nbytes / max(1, os.path.getsize(zip_path))
        except Exception:
            cr = float("nan")

        print(
            f"- {cmd_name} stats: CR={cr:.3f}  "
            f"MaxAbsErr={max_abs_err:.3f}  MAE={mae:.3f}  "
            f"RMSE={rmse:.3f}  PSNR={psnr:.3f} dB  L∞={l_inf:.3f}"
        )
        print(
            f"- Finite pairs={valid_count}/{raw.size}; "
            f"input NaN={raw_nan_count}, input Inf={raw_inf_count}, "
            f"reconstructed NaN={dec_nan_count}, reconstructed Inf={dec_inf_count}"
        )

        raw_size = os.path.getsize(raw_path) / 1024
        zip_size = os.path.getsize(zip_path) / 1024
        dec_size = os.path.getsize(dec_path) / 1024
        mytuple = (
            fname, cmd_name, num_blocks, num_generations, block_size,
            raw_size, zip_size, dec_size, elapsed_time, eps, cr,
            max_abs_err, mae, rmse, psnr, l_inf, valid_count,
            raw_nan_count, raw_inf_count, dec_nan_count, dec_inf_count
        )

        with open(results_path, "a", encoding="utf-8") as f:
            f.write(
                ", ".join(
                    f"{x:.3f}"
                    if isinstance(x, (float, np.floating))
                    else str(x)
                    for x in mytuple
                ) + "\n"
            )
        return True

    except Exception as e:
        print(f"- [WARN] {cmd_name} metric computation failed: {e}")
        return False

def main():
    ap = argparse.ArgumentParser(
        description="Compress a single .f32 file with EXPRGEN, PFPL, SZ3, and ZFP over epsilon sweeps, grouped by tool."
    )
    ap.add_argument(
        "-i", "--input",
        required=True,
        help="Input .f32 data file"
    )
    ap.add_argument(
        "-n",
        type=int,
        default=None,
        help="Limit the number of input data points processed by exprgen"
    )

    ap.add_argument(
        "-tsweep",
        required=True,
        help="Tolerance sweep: start:stop:step or start:stop:count=N"
    )
    ap.add_argument(
        "-tscale", choices=("linear", "geometric"), default="geometric",
        help="Tolerance spacing (default: geometric)"
    )
    ap.add_argument(
        "-g", "--generations", type=int, default=10,
        help="Total exprgen generations (default: 10)"
    )
    ap.add_argument(
        "-s", "--save-interval", type=int, default=4,
        help="Save exprgen results every N generations (default: 4)"
    )
    ap.add_argument(
        "-bsweep", default="250:2000:count=4",
        help="Block-size sweep (default: 250:2000:count=4)"
    )
    ap.add_argument(
        "-bscale", choices=("linear", "geometric"), default="geometric",
        help="Block-size spacing (default: geometric)"
    )
    ap.add_argument("--force", action="store_true", help="Overwrite existing outputs for each epsilon")
    args = ap.parse_args()

    # Ensure tools exist
    ex_3_dir = Path(__file__).resolve().parent.parent
    exprgen_bin = which_or_fail(str(ex_3_dir / "exprgen"))
    pfpl_compress_bin = which_or_fail("pfpl_f32_abs_compress_ser")
    pfpl_decompress_bin = which_or_fail("pfpl_f32_abs_decompress_ser")
    sz3_bin = which_or_fail("sz3")
    zfp_bin = which_or_fail("zfp")

    # check input file
    in_path = Path(args.input).expanduser().resolve()
    if not in_path.exists():
        print(f"- [WARN] Input file not found: {in_path}")
        return
    if in_path.suffix.lower() != ".f32":
        print(f"- [WARN] {in_path} does not have .f32 extension; continuing.")

    # check if filesize is a multiple of 4 (4 bytes = float)
    size_bytes = in_path.stat().st_size
    if size_bytes % 4 != 0:
        print(f"- [WARN] {in_path}: size {size_bytes} is not a multiple of 4 bytes; aborting.")
        return

    N = size_bytes // 4

    if args.n is not None and args.n <= 0:
        ap.error("-n must be > 0")
    exprgen_n = N if args.n is None else min(args.n, N)

    try:
        epsilons = parse_sweep(
            args.tsweep, args.tscale, integer=False, name="-tsweep")
        block_sizes = parse_sweep(
            args.bsweep, args.bscale, integer=True, name="-bsweep")
    except ValueError as exc:
        ap.error(str(exc))

    if not 1 <= args.generations <= 99:
        ap.error("-g/--generations must be between 1 and 99")
    if args.save_interval <= 0:
        ap.error("-s/--save-interval must be > 0")
    if any(block_size <= 0 for block_size in block_sizes):
        ap.error("-bsweep values must be > 0")
    formatted_epsilons = [f"{eps:.3f}" for eps in epsilons]
    if len(set(formatted_epsilons)) != len(formatted_epsilons):
        ap.error(
            "-tsweep contains values that collide when formatted to "
            "three decimal places"
        )

    sweep_description = (
        f"{args.tsweep} ({args.tscale}, {len(epsilons)} points)"
    )

    timestamp = datetime.now().strftime("%y%m%d-%H%M")
    base_stem = in_path.stem

    # Store all artifacts under testrpts/<input filename without .f32>.
    report_dir = ex_3_dir / "testrpts" / base_stem
    report_dir.mkdir(parents=True, exist_ok=True)
    results_path = report_dir / f"{base_stem}_{timestamp}.csv"
    with open(results_path, "w", encoding="utf-8") as f:
        header = (
            "fname", "cmd", "#blocks", "#generations", "blocksize",
            "raw.kb", "zip.kb", "dec.kb", "elapsed.s", "eps", "CR",
            "MaxAbsErr", "MAE", "RMSE", "PSNR.dB", "L_inf",
            "valid.samples", "raw.nan", "raw.inf", "dec.nan", "dec.inf"
        )
        f.write(", ".join(header) + "\n")

    logs_dir = report_dir
    outputs_dir = report_dir

    ### TESTFILE HEADING
    print("\n\n\n---------------------------------\n")
    print(f"# TEST FILE:   {str(in_path)}, N={N}\n")
    print(f"- Input:       {in_path} ({human_bytes(size_bytes)}), N={N}")
    print(f"- Exprgen N:   {exprgen_n}")
    print(f"- Sweep:       {sweep_description}")
    print(f"- Generations: {args.generations}")
    print(f"- Save every:  {args.save_interval} generations")
    print(f"- Block sizes: {block_sizes}")
    print(f"- Outputs dir: {outputs_dir}")
    print(f"- Logs dir:    {logs_dir}")
    print(f"- Results CSV: {results_path}")

    # -------------------------
    # 1) EXPRGEN — all block sizes and epsilons, with generation checkpoints
    # -------------------------
    checkpoint_generations = list(
        range(args.save_interval, args.generations + 1, args.save_interval)
    )
    if not checkpoint_generations or checkpoint_generations[-1] != args.generations:
        checkpoint_generations.append(args.generations)

    for block_size in block_sizes:
        print(
            f"\n\n## EXPRGEN BlockSize={block_size} "
            f"Generations={args.generations} "
            f"SaveInterval={args.save_interval} "
            f"eps=[{sweep_description}]\n\n"
        )
        for eps in epsilons:
            eps_tag = f"eps-{eps:.3f}_bs-{block_size}"
            base = f"{base_stem}_{timestamp}_{eps_tag}"
            fname = base_stem

            exprgen_compressed_base = outputs_dir / f"{base}.zsr"
            exprgen_dec_base = outputs_dir / f"{base}.f32"
            exprgen_log = logs_dir / f"{base}_log.md"

            checkpoint_outputs = []
            for generation in checkpoint_generations:
                generation_tag = f",g-{generation:02d}"
                compressed_path = exprgen_compressed_base.with_name(
                    f"{exprgen_compressed_base.stem}{generation_tag}"
                    f"{exprgen_compressed_base.suffix}"
                )
                dec_path = exprgen_dec_base.with_name(
                    f"{exprgen_dec_base.stem}{generation_tag}"
                    f"{exprgen_dec_base.suffix}"
                )
                checkpoint_outputs.append(
                    (generation, compressed_path, dec_path)
                )

            def normalize_checkpoint_names():
                """Rename Exprgen's native _genN checkpoints to ,g-NN."""
                for generation, compressed_path, dec_path in checkpoint_outputs:
                    for requested_base, desired_path in (
                        (exprgen_compressed_base, compressed_path),
                        (exprgen_dec_base, dec_path),
                    ):
                        native_path = requested_base.with_name(
                            f"{requested_base.stem}_gen{generation}"
                            f"{requested_base.suffix}"
                        )
                        if native_path.exists():
                            native_path.replace(desired_path)

            recorded_generations = set()

            def record_available_checkpoints(include_final=False, final_elapsed=""):
                for generation, compressed_path, dec_path in checkpoint_outputs:
                    if generation in recorded_generations:
                        continue
                    if generation == args.generations and not include_final:
                        continue
                    if not compressed_path.exists() or not dec_path.exists():
                        continue
                    if dec_path.stat().st_size != exprgen_n * np.dtype(np.float32).itemsize:
                        continue

                    appended = errorMetrics(
                        fname,
                        (
                            f"exprgen."
                            f"{(exprgen_n + block_size - 1) // block_size}."
                            f"{generation}."
                            f"{block_size}"
                        ),
                        final_elapsed if generation == args.generations else "",
                        eps,
                        in_path,
                        compressed_path,
                        dec_path,
                        results_path,
                        num_blocks=(exprgen_n + block_size - 1) // block_size,
                        num_generations=generation,
                        block_size=block_size,
                        count=exprgen_n
                    )
                    if appended:
                        recorded_generations.add(generation)

            outputs_exist = all(
                compressed_path.exists() and dec_path.exists()
                for _, compressed_path, dec_path in checkpoint_outputs
            )
            if args.force or not outputs_exist:
                cmd = [
                    exprgen_bin,
                    "-i", str(in_path),
                    "-n", str(exprgen_n),
                    "-z", str(exprgen_compressed_base),
                    "-o", str(exprgen_dec_base),
                    "-e", str(f"{eps:g}"),
                    "-b", str(block_size),
                    "-g", str(args.generations),
                    "-s", str(args.save_interval)
                ]

                print(
                    f"\n\n### EXPRGEN BlockSize={block_size} "
                    f"Generations={args.generations} "
                    f"SaveInterval={args.save_interval} eps={eps:g}\n\n"
                )
                rc, tsec = run(
                    cmd,
                    exprgen_log,
                    checkpoint_callback=record_available_checkpoints
                )
                normalize_checkpoint_names()
                status = "OK" if rc == 0 else f"FAIL({rc})"
                print(
                    f"- {status} [{tsec:.3f}s] "
                    f"(log: {exprgen_log})"
                )

                if rc != 0:
                    print("- [WARN] exprgen failed; retaining completed checkpoint metrics")
                    record_available_checkpoints(
                        include_final=True,
                        final_elapsed=tsec
                    )
                    continue
            else:
                print(
                    "- [WARN] Compression SKIPPED "
                    "(all checkpoint files exist); recalculating CSV metrics"
                    "\n\n"
                )
                tsec = ""

            record_available_checkpoints(
                include_final=True,
                final_elapsed=tsec
            )

            for generation, _, _ in checkpoint_outputs:
                if generation not in recorded_generations:
                    print(
                        f"- [WARN] Missing generation {generation} output "
                        "files; skipping its metrics"
                    )
        
    print()

    # -------------------------
    # 2) PFPL — all epsilons
    #     (PFPL serial absolute-error compression/decompression, then metrics)
    # -------------------------
    print(f"## PFPL eps=[{sweep_description}]\n\n")

    for eps in epsilons:
        eps_tag = f"eps-{eps:.3f}"
        base = f"{base_stem}_{timestamp}_{eps_tag}"
        fname = base_stem

        pfpl_zip = outputs_dir / f"{base}.pfpl"
        pfpl_dec = outputs_dir / f"{base}_pfpl_new.f32"
        pfpl_log = logs_dir / f"{base}_pfpl_log.md"

        if args.force or not pfpl_zip.exists():

            # ---- PFPL Compress ----
            ccmd = [
                pfpl_compress_bin,
                str(in_path),      # input file
                str(pfpl_zip),     # compressed .pfpl output
                f"{eps:g}",        # eps = tolerance
            ]

            print(f"\n\n## PFPL eps={eps:g}\n\n**Compression:**\n\n")

            rc, tsec = run(ccmd, pfpl_log)

            status = "OK" if rc == 0 else f"FAIL({rc})"
            print(f"- {status} [{tsec:.3f}s] -> {pfpl_zip} (log: {pfpl_log})")

            # ---- PFPL Decompress ----
            dcmd = [
                pfpl_decompress_bin,
                str(pfpl_zip),    # compressed input
                str(pfpl_dec),    # reconstructed .f32
            ]

            print(f"\n\n## PFPL eps={eps:g}\n\n**Decompression:**\n\n")
            rc, tsec = run(dcmd, pfpl_log)
            dstatus = "OK" if rc == 0 else f"FAIL({rc})"
            print(f"- {dstatus} [{tsec:.3f}s] -> {pfpl_dec} (log: {pfpl_log})")

            errorMetrics(
                fname, "PFPL", tsec, eps, in_path,
                pfpl_zip,
                pfpl_dec,
                results_path
            )

        else:
            print(f"- [WARN] Test SKIPPED (file exists: {pfpl_zip})")

    print()

    # -------------------------
    # 3) SZ3 — all epsilons
    # -------------------------
    print(f"## SZ3 eps=[{sweep_description}]\n\n")
    for eps in epsilons:
        eps_tag = f"eps-{eps:.3f}"
        base = f"{base_stem}_{timestamp}_{eps_tag}"
        fname = base_stem

        sz3_zip = outputs_dir / f"{base}.sz3"
        sz3_dec = outputs_dir / f"{base}_sz3_new.f32"
        sz3_log = logs_dir / f"{base}_sz3_log.md"

        if args.force or not sz3_zip.exists():
            ccmd = [
                sz3_bin, "-f",
                "-i", str(in_path),
                "-z", str(sz3_zip),
                "-1", str(N),
                "-M", "ABS", f"{eps:g}",
                "-a"
            ]
            print(f"\n\n## SZ3 eps={eps:g}\\n**Compression:**\n\n")
            rc, tsec = run(ccmd, sz3_log)
            status = "OK" if rc == 0 else f"FAIL({rc})"
            print(f"- {status} [{tsec:.3f}s] -> {sz3_zip} (log: {sz3_log})")

            dcmd = [sz3_bin,
                    "-f",
                    "-z", str(sz3_zip),
                    "-o", str(sz3_dec),
                    "-x"]

            print(f"\n\n## SZ3 eps={eps:g}\\n**Decompression:**\n\n")
            rc, tsec = run(dcmd, sz3_log)
            dstatus = "OK" if rc == 0 else f"FAIL({rc})"
            print(f"- {dstatus} [{tsec:.3f}s] -> {sz3_dec} (log: {sz3_log})")
            errorMetrics(
                fname, "SZ3", tsec, eps, in_path,
                sz3_zip,
                sz3_dec,
                results_path
            )

        else:
            print(f"- [WARN] Test SKIPPED (file exists: {sz3_zip})")

    print()

    # -------------------------
    # 4) ZFP — all epsilons
    # -------------------------
    print(f"## ZFP eps=[{sweep_description}]\n\n")
    for eps in epsilons:
        eps_tag = f"eps-{eps:.3f}"
        base = f"{base_stem}_{timestamp}_{eps_tag}"
        fname = base_stem

        zfp_zip = outputs_dir / f"{base}.zfp"
        zfp_dec = outputs_dir / f"{base}_new.f32"

        zfp_log = logs_dir / f"{base}_zfp_log.md"

        if args.force or not zfp_zip.exists():
            zcmd = [
                zfp_bin, "-f",
                "-i", str(in_path),
                "-z", str(zfp_zip),
                "-t", "f32",
                "-1", str(N),
                "-a", f"{eps:g}",
                "-s","-h"
            ]

            print(f"\n\n## ZFP eps={eps:g}\\n**Compression:**\n\n")
            rc, tsec = run(zcmd, zfp_log)
            status = "OK" if rc == 0 else f"FAIL({rc})"

            print(f"- {status} [{tsec:.3f}s] -> {zfp_zip} (log: {zfp_log})")

            # zfp decompress
            zcmd = [
                zfp_bin,"-h",
                "-z", str(zfp_zip),
                "-o", str(zfp_dec)
            ]
            print(f"\n\n## ZFP eps={eps:g}\\n**Deompression:**\n\n")
            rc, tsec = run(zcmd, zfp_log)
            status = "OK" if rc == 0 else f"FAIL({rc})"
            print(f"- {status} [{tsec:.3f}s] -> {zfp_zip} (log: {zfp_log})")

            errorMetrics(
                fname, "ZFP", tsec, eps, in_path,
                zfp_zip,
                zfp_dec,
                results_path
            )
        else:
            print(f"- [WARN] Test SKIPPED (file exists: {zfp_zip})")

    print("\n## All Tests Completed")

if __name__ == "__main__":
    main()

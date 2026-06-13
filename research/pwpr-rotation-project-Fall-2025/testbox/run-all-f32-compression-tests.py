#!/usr/bin/env python3
"""
compress_all_f32.py

Loop through all *.f32 files in the current directory and run:
  1) pwl  <input> <epserance>
  2) sz3  -z -i <input> -o <output.sz>  -t f32 -a <eps> -1 N --print-stats
  3) zfp  -f -i <input> -o <output.zfp> -t f32 -a <eps> -1 N --stats

Usage:
  python3 compress_all_f32.py --epsilon 0.01 [--force]

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
import time

from pathlib import Path

import struct
from typing import Tuple

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

def run(cmd, log_path):

    start = time.time()
    proc = subprocess.run(cmd, capture_output=True, text=True)
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

        if proc.stdout:
            #print("### ---- STDOUT ----\n")
            print(f"```\n[stdout] from command: {cmd[0]}\n")
            print(proc.stdout)
            print("\n```\n")

            #f.write("### ---- STDOUT ----\n")
            f.write(f"```\n[stdout] from command: {cmd[0]}\n")
            f.write(proc.stdout)
            f.write("\n```\n")

        if proc.stderr:
            #print("### ---- STDERR ----\n")
            print(f"```\n[stderr] from command: {cmd[0]}\n")
            print(proc.stderr)
            print("\n```\n")

            #f.write("### ---- STDERR ----\n")
            f.write(f"```\n[stderr] from command: {cmd[0]}\n")
            f.write(proc.stderr)
            f.write("\n```\n")

    return proc.returncode, elapsed

import struct
from typing import Tuple

def getDegreeCounts(filename: str) -> Tuple[int, int, int, int, int]:
    """
    Read binary segment records produced by writeSegmentsBinary().

    File layout per record (little-endian):
        uint32  body_size_prefix   (4 bytes, not counted in body)
        uint64  start              (8 bytes)
        uint64  end                (8 bytes)
        uint32  degree             (4 bytes)
        float   coeffs[5]          (20 bytes)
    """

    prefix_fmt = "<I"         # uint32_t record_size prefix (body size)
    body_fmt   = "<QQI5f"     # uint64_t, uint64_t, uint32_t, 5 floats

    prefix_size = struct.calcsize(prefix_fmt)  # 4
    body_size   = struct.calcsize(body_fmt)    # 40

    count = [0, 0, 0, 0, 0]   # total, deg1, deg2, deg3, deg4
    recnum = 0

    with open(filename, "rb") as f:
        while True:
            # 1) Read 4-byte size prefix
            prefix = f.read(prefix_size)
            if not prefix:
                break  # normal EOF

            if len(prefix) != prefix_size:
                raise ValueError(f"Truncated record_size prefix at record #{recnum}")

            (sz,) = struct.unpack(prefix_fmt, prefix)

            # Sanity check: sz must match body_size (40)
            if sz != body_size:
                raise ValueError(
                    f"Record size mismatch at record #{recnum}: "
                    f"file says {sz}, Python expects {body_size}"
                )

            # 2) Read the record body
            body = f.read(body_size)
            if len(body) != body_size:
                raise ValueError(
                    f"Truncated record body at record #{recnum}: "
                    f"expected {body_size}, got {len(body)}"
                )

            start, end, degree, *coeffs = struct.unpack(body_fmt, body)

            # 3) Update counts
            count[0] += 1
            if 1 <= degree <= 4:
                count[degree] += 1

            recnum += 1

    return tuple(count)


def errorMetrics(fname, cmd_name, elapsed_time, eps, raw_path, zip_path, dec_path, counts):
    print(f"\n\nError metrics for command: {cmd_name}\n\n")

    try:
        raw = np.fromfile(str(raw_path), dtype=np.float32)
        dec = np.fromfile(str(dec_path), dtype=np.float32)

        if dec.size != raw.size:
            print(f"- [WARN] decompressed size {dec.size} != raw size {raw.size}; skipping metrics.")

        else:
            err = dec - raw
            abs_err = np.abs(err)

            mae  = float(abs_err.mean())
            rmse = float(np.sqrt((err**2).mean()))
            l_inf = float(abs_err.max())

            data_range = float(raw.max() - raw.min())
            if data_range == 0.0:
                psnr = float("inf")
            else:
                mse = float((err**2).mean())
                psnr = (20 * np.log10(data_range) -
                        10 * np.log10(mse)) if mse > 0 else float("inf")

            try:
                cr = (raw.nbytes) / max(1, os.path.getsize(zip_path))
            except Exception:
                cr = float("nan")

            print(f"- {cmd_name} stats: CR={cr:.2f}  "
                  f"MAE={mae:.6g}  RMSE={rmse:.6g}  L∞={l_inf:.6g}  PSNR={psnr:.2f} dB")

            raw_size=os.path.getsize(raw_path)/1024
            zip_size=os.path.getsize(zip_path)/1024
            dec_size=os.path.getsize(dec_path)/1024
            mytuple=(fname,cmd_name, raw_size, zip_size, dec_size, elapsed_time, eps, cr, mae, rmse, l_inf, psnr,     
                counts[0],counts[1],counts[2],counts[3],counts[4])

            # print(f"\n\n###### CSV {tuple}\n")
            with open("results.csv", "a") as f:
                f.write(", ".join(f"{x:.3f}" if isinstance(x, float) else str(x) for x in mytuple) + "\n")

    except Exception as e:
        print(f"- [WARN] {cmd_name} metric computation failed: {e}")

def main():
    ap = argparse.ArgumentParser(
        description="Compress a single .f32 file with PWLR, PWPR, PFPL, SZ3, and ZFP over epsilon sweeps, grouped by tool."
    )
    ap.add_argument("-f", "--file", required=True, help="Input .f32 filename")

    ap.add_argument("-tfrom", type=float, required=True, help="Starting epsilon (inclusive)")
    ap.add_argument("-tto", type=float, required=True, help="Ending epsilon (inclusive if exact)")
    ap.add_argument("-tstep", type=float, required=True, help="Tolerance step")
    ap.add_argument("--force", action="store_true", help="Overwrite existing outputs for each epsilon")
    args = ap.parse_args()

    # setup results.csv header
    with open("./results.csv", "a") as f:
        tuple = ("fname", "cmd", "raw.kb", "zip.kb", "dec.kb", "elapsed.ms", "eps", "CR", "MAE", "RMSE", "L.inf", "PSNR.dB", "tot-segs", "deg-1", "deg-2","deg-3","deg-4")
        f.write(", ".join(map(str, tuple)) + "\n")

    # Ensure tools exist
    pwlr_bin = which_or_fail("pwlr")
    pwpr_bin = which_or_fail("pwpr")
    pfplcom_bin  = which_or_fail("pfplcom")
    pfpldecom_bin = which_or_fail("pfpldecom")
    sz3_bin = which_or_fail("sz3")
    zfp_bin = which_or_fail("zfp")

    # check input file
    in_path = Path(args.file)
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

    # Build a stable list of epsilons once (avoid float drift across loops)
    from decimal import Decimal, getcontext

    getcontext().prec = 28
    t_from = Decimal(str(args.tfrom))
    t_to   = Decimal(str(args.tto))
    t_step = Decimal(str(args.tstep))

    if t_step <= 0:
        print("- tstep must be > 0")
        return
    if t_from > t_to:
        print("- tfrom must be <= tto")
        return

    epsilons = []
    t = t_from
    while t <= t_to + Decimal("1e-28"):  # tiny epsilon
        epsilons.append(float(t))
        t += t_step

    timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    base_stem = in_path.stem

    logs_dir = Path("logs")
    logs_dir.mkdir(exist_ok=True)

    tmp_outputs_dir = Path("/tmp/outputs")
    tmp_outputs_dir.mkdir(exist_ok=True)
    outputs_dir = str(tmp_outputs_dir) + "/"

    ### TESTFILE HEADING
    print("\n\n\n---------------------------------\n")
    print(f"# TEST FILE:   {str(in_path)}, N={N}\n")
    print(f"- Input:       {in_path} ({human_bytes(size_bytes)}), N={N}")
    print(f"- Sweep:       from {epsilons[0]:g} to {epsilons[-1]:g} step {args.tstep:g}")
    print(f"- Outputs dir: {outputs_dir}")
    print(f"- Logs dir:    {logs_dir}") 

    # -------------------------
    # 1) PWLR — all epsilons
    # -------------------------
    print(f"\n\n## PWLR eps=[{args.tfrom}:{args.tto}:{args.tstep}]\n\n")
    for eps in epsilons:
        eps_tag = f"eps-{eps:g}"
        base = f"{base_stem}_{timestamp}_{eps_tag}"
        fname = base_stem
        pwlr_zip = Path(f"{base}.pwlr")
        pwlr_dec = Path(f"{base}_new.f32")
        pwlr_log = logs_dir / f"{base}_pwlr.log"

        if args.force or not pwlr_zip.exists():
            cmd = [
                pwlr_bin,
                "-i", str(in_path),
                "-z", outputs_dir + str(pwlr_zip),
                "-o", outputs_dir + str(pwlr_dec),
                "-e", str(f"{eps:g}")
            ]

            print(f"\n\n### PWLR  {eps:g}\n\n")
            rc, tsec = run(cmd, pwlr_log)
            status = "OK" if rc == 0 else f"FAIL({rc})"
            print(f"- {status} [{tsec:.3f}s] (log: {pwlr_log})")

            degreeOneCount=Path(outputs_dir + str(pwlr_zip)).stat().st_size//struct.calcsize("<iiff")
            counts=(degreeOneCount,0,0,0,0)

            print(f"\n\n### PWLR ZIP {eps:g}\n\n")
            zipfile=outputs_dir+"pwlr.zip"
            zcmd=["zip",
                 zipfile,
                 outputs_dir+str(pwlr_zip)]
            rc, tsec = run(zcmd, pwlr_log)
            status = "OK" if rc == 0 else f"FAIL({rc})"
            print(f"- {status} [{tsec:.3f}s] (log: {pwlr_log})")

            errorMetrics(fname,"PWLR",tsec,eps,in_path, zipfile, outputs_dir + str(Path(pwlr_dec)),counts)

            if os.path.exists(zipfile):
                os.remove(zipfile)
            else:
                print(f"File ({zipfile}) does not exist.")

        else:
            print(f"- [WARN] Test SKIPPED (file exists: {outputs_dir}{pwlr_zip})\n\n")
        
    print()

    # -------------------------
    # 1.1) pwpr — all epsilons
    # -------------------------
    for degree in range(0,4):
        print(f"\n\n## PWPR Degree={degree} eps=[{args.tfrom}:{args.tto}:{args.tstep}]\n\n")
        for eps in epsilons:
            eps_tag = f"eps-{eps:g}_deg-{degree}"
            base = f"{base_stem}_{timestamp}_{eps_tag}"
            fname = base_stem

            pwpr_zip = Path(f"{base}.pwpr")
            pwpr_dec = Path(f"{base}_new.f32")
            pwpr_log = logs_dir / f"{base}_pwpr.log"

            counts=(0,0,0,0,0)

            if args.force or not pwpr_zip.exists():
                cmd = [
                    pwpr_bin,
                    "-i", str(in_path),
                    "-z", outputs_dir + str(pwpr_zip),
                    "-o", outputs_dir + str(pwpr_dec),
                    "-e", str(f"{eps:g}"),
                    f"-{degree}"
                ]

                print(f"\n\n### PWPR Degree={degree}  {eps:g}\n\n")
                rc, tsec = run(cmd, pwpr_log)
                status = "OK" if rc == 0 else f"FAIL({rc})"
                print(f"- {status} [{tsec:.3f}s] (log: {pwpr_log})")

                counts=getDegreeCounts(outputs_dir + str(pwpr_zip))
            else:
                print(f"- [WARN] Test SKIPPED (file exists: {outputs_dir}{pwpr_zip})\n\n")
        
       
            print(f"\n\n### PWPR ZIP Degree={degree} eps={eps:g}\n\n")
            zipfile=outputs_dir+"pwpr.zip"
            zcmd=["zip",
                zipfile,
                outputs_dir + str(pwpr_zip)]
            rc, zsec = run(zcmd, pwpr_log)
            status = "OK" if rc == 0 else f"FAIL({rc})"
            print(f"- {status} [{zsec:.3f}s] (log: {pwpr_log})")

            errorMetrics(fname,f"PWPR({degree})",tsec,eps,in_path, zipfile, outputs_dir+str(Path(pwpr_dec)),counts)
            
            if os.path.exists(zipfile):
                os.remove(zipfile)
            else:
                print("File ({zipfile}) does not exist.")


        else:
            print(f"- [WARN] Test SKIPPED (file exists: {outputs_dir}{pwpr_zip})\n\n")
        
    print()

    # -------------------------
    # 2) PFPL — all epsilons
    #     (pfplcom to compress, pfpldecom to decompress, then metrics)
    # -------------------------
    print(f"## PFPL eps=[{args.tfrom}:{args.tto}:{args.tstep}]\n\n")

    # Tool locations (ensure these exist)
    pfplcom_bin  = which_or_fail("pfplcom")
    pfpldecom_bin = which_or_fail("pfpldecom")

    for eps in epsilons:
        eps_tag = f"eps-{eps:g}"
        base = f"{base_stem}_{timestamp}_{eps_tag}"
        fname = base_stem

        pfpl_zip = Path(f"{base}.pfpl")
        pfpl_dec = Path(f"{base}_pfpl_new.f32")
        pfpl_log = logs_dir / f"{base}_pfpl.log"

        if args.force or not pfpl_zip.exists():

            # ---- PFPL Compress ----
            ccmd = [
                pfplcom_bin,
                str(in_path),      # input file
                outputs_dir + str(pfpl_zip),     # compressed .pfpl output
                f"{eps:g}",        # eps = tolerance
            ]

            print(f"\n\n## PFPL eps={eps:g}\n\n**Compression:**\n\n")

            rc, tsec = run(ccmd, pfpl_log)

            status = "OK" if rc == 0 else f"FAIL({rc})"
            print(f"- {status} [{tsec:.3f}s] -> {pfpl_zip} (log: {pfpl_log})")

            # ---- PFPL Decompress ----
            dcmd = [
                pfpldecom_bin,
                outputs_dir + str(pfpl_zip),    # compressed input
                outputs_dir + str(pfpl_dec),    # reconstructed .f32
            ]

            print(f"\n\n## PFPL eps={eps:g}\n\n**Decompression:**\n\n")
            rc, tsec = run(dcmd, pfpl_log)
            dstatus = "OK" if rc == 0 else f"FAIL({rc})"
            print(f"- {dstatus} [{tsec:.3f}s] -> {pfpl_dec} (log: {pfpl_log})")

            counts=(0,0,0,0,0)
            errorMetrics(fname,"PFPL",tsec,eps,in_path, outputs_dir + str(pfpl_zip), outputs_dir + str(pfpl_dec),counts)

        else:
            print(f"- [WARN] Test SKIPPED (file exists: {outputs_dir}{pfpl_zip})")

    print()

    # -------------------------
    # 3) SZ3 — all epsilons
    # -------------------------
    print(f"## SZ3 eps=[{args.tfrom}:{args.tto}:{args.tstep}]\n\n")
    for eps in epsilons:
        eps_tag = f"eps-{eps:g}"
        base = f"{base_stem}_{timestamp}_{eps_tag}"
        fname = base_stem

        sz3_zip = Path(f"{base}.sz3")
        sz3_dec = Path(f"{base}_sz3_new.f32")
        sz3_log = logs_dir / f"{base}_sz3.log"

        if args.force or not sz3_zip.exists():
            ccmd = [
                sz3_bin, "-f",
                "-i", str(in_path),
                "-z", outputs_dir + str(sz3_zip),
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
                    "-z", outputs_dir + str(sz3_zip),
                    "-o", outputs_dir + str(sz3_dec),
                    "-x"]

            print(f"\n\n## SZ3 eps={eps:g}\\n**Decompression:**\n\n")
            rc, tsec = run(dcmd, sz3_log)
            dstatus = "OK" if rc == 0 else f"FAIL({rc})"
            print(f"- {dstatus} [{tsec:.3f}s] -> {sz3_dec} (log: {sz3_log})")
            counts=(0,0,0,0,0)
            errorMetrics(fname,"SZ3",tsec,eps,in_path, outputs_dir + str(sz3_zip), outputs_dir + str(sz3_dec),counts)

        else:
            print(f"- [WARN] Test SKIPPED (file exists: {outputs_dir}{sz3_zip})")

    print()

    # -------------------------
    # 4) ZFP — all epsilons
    # -------------------------
    print(f"## ZFP eps=[{args.tfrom}:{args.tto}:{args.tstep}]\n\n")
    for eps in epsilons:
        eps_tag = f"eps-{eps:g}"
        base = f"{base_stem}_{timestamp}_{eps_tag}"
        fname = base_stem

        zfp_zip = Path(f"{base}.zfp")
        zfp_dec = Path(f"{base}_new.f32")

        zfp_log = logs_dir / f"{base}_zfp.log"

        if args.force or not zfp_zip.exists():
            zcmd = [
                zfp_bin, "-f",
                "-i", str(in_path),
                "-z", outputs_dir + str(zfp_zip),
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
                "-z", outputs_dir + str(zfp_zip),
                "-o", outputs_dir + str(zfp_dec)
            ]
            print(f"\n\n## ZFP eps={eps:g}\\n**Deompression:**\n\n")
            rc, tsec = run(zcmd, zfp_log)
            status = "OK" if rc == 0 else f"FAIL({rc})"
            print(f"- {status} [{tsec:.3f}s] -> {zfp_zip} (log: {zfp_log})")

            counts=(0,0,0,0,0)
            errorMetrics(fname,"ZFP",tsec,eps,in_path, outputs_dir + str(zfp_zip), outputs_dir + str(zfp_dec),counts)
        else:
            print(f"- [WARN] Test SKIPPED (file exists: {outputs_dir}{zfp_zip})")

    print("\n## All Tests Completed")

if __name__ == "__main__":
    main()

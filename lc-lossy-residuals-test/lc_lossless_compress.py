#!/usr/bin/env python3
"""Benchmark f32 lossless codecs and append measurements to a shared CSV."""
from __future__ import annotations
import argparse
import csv
import fcntl
import filecmp
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import time
import lc_two_part_compress as lc

FIELDS = ["file", "command", "command_args", "original_bytes", "compressed_bytes",
          "compression_ratio", "exact_reconstruction", "compression_time_seconds",
          "compression_throughput_MB_s", "decompression_time_seconds",
          "decompression_throughput_MB_s"]
TOOLS = ["lc", "sz3", "zfp", "pfpl", "bzip2", "zstandard", "sleek", "spspeed", "spratio", "zip"]


def build_parser():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("input", help="Nonempty raw binary f32 file")
    p.add_argument("--csv", default=str(Path(__file__).resolve().parent / "testrpts/lossless_results.csv"),
                   help="Common CSV; append one row per codec/settings combination")
    p.add_argument("--tools", nargs="+", choices=TOOLS, default=TOOLS)
    p.add_argument("--lc-dir", default=".")
    p.add_argument("--lossless-pattern", default=lc.DEFAULT_PATTERN)
    p.add_argument("--compiler", default=os.environ.get("CXX", "g++-16"))
    p.add_argument("--bzip2-levels", nargs="+", type=int, default=[1, 5, 9])
    p.add_argument("--zstd-levels", nargs="+", type=int, default=[1, 3, 9, 19, 22])
    p.add_argument("--zip-levels", nargs="+", type=int, default=[1, 6, 9])
    for flag, default in {
        "sz3": "sz3", "zfp": "zfp", "pfpl-compressor": "pfpl_f32_abs_compress_ser",
        "pfpl-decompressor": "pfpl_f32_abs_decompress_ser", "bzip2": "bzip2",
        "zstd": "zstd", "zip": "zip", "unzip": "unzip",
        "sleek-compressor": str(Path.home()/"bin/sleek_single_compressor_lossless_cpu"),
        "sleek-decompressor": str(Path.home()/"bin/sleek_single_decompressor_lossless_cpu"),
        "spspeed-compressor": str(Path.home()/"bin/speed-cpu-compress-single.exe"),
        "spspeed-decompressor": str(Path.home()/"bin/speed-cpu-decompress-single.exe"),
        "spratio-compressor": str(Path.home()/"bin/ratio-cpu-compress-single.exe"),
        "spratio-decompressor": str(Path.home()/"bin/ratio-cpu-decompress-single.exe"),
    }.items():
        p.add_argument("--"+flag, default=default)
    return p


def executable(value):
    path = shutil.which(str(Path(value).expanduser()))
    if path is None:
        raise ValueError(f"Executable not found: {value}")
    return str(Path(path).resolve())


def validate(args):
    inp = Path(args.input).expanduser().resolve()
    if not inp.is_file() or inp.stat().st_size == 0 or inp.stat().st_size % 4:
        raise ValueError("Input must be a nonempty raw f32 file (size divisible by 4)")
    for levels, maximum, label in [(args.bzip2_levels,9,"Bzip2"),
                                    (args.zstd_levels,22,"Zstandard"), (args.zip_levels,9,"ZIP")]:
        if any(not 1 <= level <= maximum for level in levels):
            raise ValueError(f"{label} levels must be between 1 and {maximum}")
    required = {"sz3":["sz3"], "zfp":["zfp"],
                "pfpl":["pfpl_compressor","pfpl_decompressor"],
                "bzip2":["bzip2"], "zstandard":["zstd"], "zip":["zip","unzip"]}
    for tool in ["sleek","spspeed","spratio"]:
        required[tool] = [tool+"_compressor",tool+"_decompressor"]
    for tool in args.tools:
        for attribute in required.get(tool,[]):
            setattr(args,attribute,executable(getattr(args,attribute)))
    args.lc_dir = Path(args.lc_dir).expanduser().resolve()
    if "lc" in args.tools:
        executable(str(args.lc_dir/"lc"))
        executable(args.compiler)
        if not (args.lc_dir/"generate_standalone_CPU_compressor_decompressor.py").is_file():
            raise ValueError("LC CPU standalone generator not found")
    return inp


def timed(command, cwd, stdout_file=None):
    # Include process launch, codec execution, and file I/O; exclude search/build/verification.
    with (stdout_file.open("wb") if stdout_file else tempfile.TemporaryFile()) as output:
        start = time.perf_counter()
        result = subprocess.run(command, cwd=cwd, stdout=output, stderr=subprocess.PIPE)
        elapsed = time.perf_counter() - start
    if result.returncode:
        raise RuntimeError(f"Command failed: {shlex.join(command)}\n"
                           + result.stderr.decode(errors="replace"))
    return elapsed


def append_row(path, row):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a+", newline="", encoding="utf-8") as output:
        fcntl.flock(output, fcntl.LOCK_EX)
        try:
            output.seek(0)
            header = next(csv.reader(output), None)
            if header is not None and header != FIELDS:
                raise ValueError(f"CSV header does not match expected fields: {path}")
            output.seek(0, os.SEEK_END)
            writer = csv.DictWriter(output, fieldnames=FIELDS)
            if header is None:
                writer.writeheader()
            writer.writerow(row)
            output.flush()
            os.fsync(output.fileno())
        finally:
            fcntl.flock(output, fcntl.LOCK_UN)


def commands(args, tool, inp, compressed, decoded, level=None):
    count = str(inp.stat().st_size//4)
    cwd = compressed.parent
    if tool == "sz3":
        return ([args.sz3,"-f","-i",str(inp),"-z",str(compressed),"-1",count,"-M","ABS","0","-a"],
                [args.sz3,"-f","-z",str(compressed),"-o",str(decoded),"-1",count], None,None,cwd)
    if tool == "zfp":
        return ([args.zfp,"-h","-f","-1",count,"-i",str(inp),"-z",str(compressed),"-R","-s"],
                [args.zfp,"-h","-z",str(compressed),"-o",str(decoded)],None,None,cwd)
    if tool == "pfpl":
        return ([args.pfpl_compressor,str(inp),str(compressed),"0"],
                [args.pfpl_decompressor,str(compressed),str(decoded)],None,None,cwd)
    if tool in ["sleek","spspeed","spratio"]:
        return ([getattr(args,tool+"_compressor"),str(inp),str(compressed)],
                [getattr(args,tool+"_decompressor"),str(compressed),str(decoded)],None,None,cwd)
    if tool == "bzip2":
        return ([args.bzip2,f"-{level}","-c",str(inp)],
                [args.bzip2,"-d","-c",str(compressed)],compressed,decoded,cwd)
    if tool == "zstandard":
        return ([args.zstd,"--ultra",f"-{level}","-q","-f",str(inp),"-o",str(compressed)],
                [args.zstd,"-d","-q","-f",str(compressed),"-o",str(decoded)],None,None,cwd)
    if tool == "zip":
        return ([args.zip,"-q",f"-{level}","-j",str(compressed),"--",str(inp)],
                [args.unzip,"-p",str(compressed)],None,decoded,cwd)
    raise ValueError(tool)


def measure(inp, compressed, decoded, encode, decode, cwd, encode_stdout=None, decode_stdout=None):
    compression_time = timed(encode,cwd,encode_stdout)
    decompression_time = timed(decode,cwd,decode_stdout)
    exact = filecmp.cmp(inp,decoded,shallow=False)
    original_bytes = inp.stat().st_size
    compressed_bytes = compressed.stat().st_size
    if compressed_bytes == 0:
        raise RuntimeError("Compressor produced an empty file")
    return {"file": inp.name, "command": encode[0], "command_args": shlex.join(encode[1:]),
            "original_bytes": original_bytes, "compressed_bytes": compressed_bytes,
            "compression_ratio": original_bytes/compressed_bytes,
            "exact_reconstruction": exact, "compression_time_seconds": compression_time,
            "compression_throughput_MB_s": original_bytes/1_000_000/compression_time,
            "decompression_time_seconds": decompression_time,
            "decompression_throughput_MB_s": original_bytes/1_000_000/decompression_time}


def main():
    args = build_parser().parse_args()
    inp = validate(args)
    csv_path = Path(args.csv).expanduser().resolve()
    # Check the header before executing expensive searches or compressors.
    if csv_path.exists() and csv_path.stat().st_size:
        with csv_path.open(newline="",encoding="utf-8") as stream:
            if next(csv.reader(stream),None) != FIELDS:
                raise ValueError(f"CSV header does not match expected fields: {csv_path}")
    with tempfile.TemporaryDirectory(prefix="lossless-benchmark-") as scratch:
        scratch = Path(scratch)
        for tool in dict.fromkeys(args.tools):
            levels = {"bzip2":args.bzip2_levels,"zstandard":args.zstd_levels,
                      "zip":args.zip_levels}.get(tool,[None])
            for level in dict.fromkeys(levels):
                run_dir = scratch/f"{tool}-{level}"
                run_dir.mkdir()
                compressed = run_dir/("compressed.zip" if tool=="zip" else "compressed.bin")
                decoded = run_dir/"decoded.raw"
                if tool == "lc":
                    lc.initialize_command_log(run_dir/"build.log",inp)
                    pipeline = lc.search_best_pipeline(args.lc_dir/"lc",inp,"",args.lossless_pattern,cwd=run_dir)
                    generated = lc.generate_sources(args.lc_dir,"",pipeline)
                    encoder,decoder = lc.preserve_and_build_pair("lossless",*generated,run_dir,args.compiler,args.lc_dir)
                    encode,decode = [str(encoder),str(inp),str(compressed)],[str(decoder),str(compressed),str(decoded)]
                    enc_out,dec_out,cwd = None,None,args.lc_dir
                else:
                    encode,decode,enc_out,dec_out,cwd = commands(args,tool,inp,compressed,decoded,level)
                row = measure(inp,compressed,decoded,encode,decode,cwd,enc_out,dec_out)
                # LC's generated executable path is temporary; preserve its pipeline in args.
                if tool == "lc":
                    row["command"] = "LC lossless standalone"
                    row["command_args"] += " ; pipeline=" + shlex.quote(pipeline)
                append_row(csv_path,row)
                print(f"{tool} {level if level is not None else ''}: "
                      f"{row['compression_ratio']:.4f}x; exact={row['exact_reconstruction']}",flush=True)
                if not row["exact_reconstruction"]:
                    raise RuntimeError(f"{tool} did not reconstruct the input exactly; failure recorded in CSV")
    print(f"CSV: {csv_path}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError,RuntimeError,OSError) as error:
        print(f"ERROR: {error}",file=sys.stderr)
        raise SystemExit(1)

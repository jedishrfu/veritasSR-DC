#!/usr/bin/env python3
"""
gen_le_f32_floats.py
====================

PURPOSE
-------
Generate values from a mathematical expression y = f(x), convert them to
IEEE-754 float32, and write them as raw little-endian binary data.

The script is useful for scientific and DSP test datasets, lookup tables,
reference signals, raw-data reader tests, and exploring functions over a
configurable domain. It can add reproducible random variation, print or save
statistics, and display an interactive plot.

REQUIREMENTS
------------
Python 3 with NumPy and Matplotlib:

    python3 -m pip install numpy matplotlib

USAGE
-----
    python3 gen_le_f32_floats.py EXPR N OUTFILE [OPTIONS]

POSITIONAL ARGUMENTS
--------------------
EXPR
    A quoted mathematical expression evaluated once per x value, such as
    "3*x + 5", "sin(x)", "sqrt(x)", "log10(x)", or "sin(2*pi*x)".

    Available names include x, math, sin, cos, tan, asin, acos, atan, sqrt,
    log, log10, exp, pow, abs, pi, and e.

N
    Positive number of samples. Each float32 sample occupies four bytes.

OUTFILE
    Destination raw binary file. A .f32 extension is recommended. The file
    contains consecutive four-byte values with no header or metadata.

OPTIONS
-------
--xstart VALUE
    First x value (default 0.0). Use it to choose the function's domain; for
    example, log10(x) requires x > 0, so use --xstart 1.

--xstep VALUE
    Difference between consecutive x values (default 1.0). Sample i uses
    x = xstart + i*xstep. Small steps sample smoothly; negative steps traverse
    backward. Zero is rejected.

--tolerance FRACTION
    Uniform random noise as a fraction: 0.01 means +/-1%, 0.025 means +/-2.5%.
    Noise uses max(abs(y), 1.0), allowing values near zero to receive noise.

--seed INTEGER
    Seed for exactly reproducible random noise.

--stats
    Print statistics in addition to the automatic Markdown report.

--plot
    Open an interactive plot. Left/right page through samples; up/down zoom.

--guide
    Print this complete guide and exit; positional arguments are not required.

-h, --help
    Print concise command help and exit; positional arguments are not required.

GENERATED FILES
---------------
OUTFILE contains raw little-endian float32 values. OUTFILE_stats.md records
settings, x/base/output statistics, optional noise statistics, a preview, and
the generation command.

DOMAIN NOTES
------------
The expression must be valid for every generated x value. Common restrictions:

    log(x), log10(x)    x > 0
    sqrt(x)             x >= 0
    1/x                 x != 0
    asin(x), acos(x)    -1 <= x <= 1
    tan(x)              undefined at pi/2 + k*pi

An evaluation failure reports the sample number and offending x value.

EXAMPLES
--------
Generate log10 values beginning at a valid x:

    python3 gen_le_f32_floats.py "log10(x)" 50000 log.f32 --xstart 1

Generate and plot a smoothly sampled sine function:

    python3 gen_le_f32_floats.py "sin(x)" 50000 sin.f32 \
        --xstart 0 --xstep 0.001 --plot

Generate reproducible data with +/-2.5% noise:

    python3 gen_le_f32_floats.py "3*x + 5" 10000 linear.f32 \
        --tolerance 0.025 --seed 12345 --stats --plot

Generate reciprocal values without evaluating 1/0:

    python3 gen_le_f32_floats.py "1/x" 1000 reciprocal.f32 \
        --xstart 1 --xstep 0.1

Print concise help or this full guide:

    python3 gen_le_f32_floats.py --help
    python3 gen_le_f32_floats.py --guide
"""
import argparse
import math
import random
import struct
import sys
from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt


GUIDE_TEXT = __doc__.strip()

SAFE_GLOBALS = {
    "__builtins__": {},
    "math": math,
    "sin": math.sin,
    "cos": math.cos,
    "tan": math.tan,
    "asin": math.asin,
    "acos": math.acos,
    "atan": math.atan,
    "sqrt": math.sqrt,
    "log": math.log,
    "log10": math.log10,
    "exp": math.exp,
    "pow": pow,
    "abs": abs,
    "pi": math.pi,
    "e": math.e,
}


def generate_values(expr, n, tolerance=0.0, xstart=0.0, xstep=1.0):
    try:
        code = compile(expr, "<expr>", "eval")
    except SyntaxError as exc:
        raise ValueError(f"Invalid expression {expr!r}: {exc.msg}") from exc

    x_values = np.empty(n, dtype=np.float32)
    base_values = np.empty(n, dtype=np.float32)
    output_values = np.empty(n, dtype=np.float32)

    for i in range(n):
        x = xstart + i * xstep
        x_values[i] = np.float32(x)

        try:
            y = float(eval(code, SAFE_GLOBALS, {"x": x}))
        except Exception as exc:
            raise RuntimeError(
                f"Expression {expr!r} failed at sample {i}, x={x}: {exc}"
            ) from exc

        base_values[i] = np.float32(y)

        noisy_y = y

        if tolerance > 0.0:
            scale = max(abs(y), 1.0)
            delta = scale * tolerance
            noisy_y += random.uniform(-delta, delta)

        output_values[i] = np.float32(noisy_y)

    return x_values, base_values, output_values


def write_f32_le(filename, values):
    with open(filename, "wb") as f:
        for y in values:
            f.write(struct.pack("<f", float(y)))


def stats_dict(values):
    finite = values[np.isfinite(values)]

    if len(finite) == 0:
        return {
            "count": len(values),
            "finite": 0,
            "min": None,
            "max": None,
            "mean": None,
            "stddev": None,
            "first": None,
            "last": None,
        }

    return {
        "count": len(values),
        "finite": len(finite),
        "min": float(np.min(finite)),
        "max": float(np.max(finite)),
        "mean": float(np.mean(finite)),
        "stddev": float(np.std(finite)),
        "first": float(values[0]),
        "last": float(values[-1]),
    }


def print_stats(values, label):
    s = stats_dict(values)

    print(f"\n{label} statistics")
    print("-" * (len(label) + 11))
    print(f"count:     {s['count']}")
    print(f"finite:    {s['finite']}")

    if s["finite"] == 0:
        print("No finite values.")
        return

    print(f"min:       {s['min']}")
    print(f"max:       {s['max']}")
    print(f"mean:      {s['mean']}")
    print(f"stddev:    {s['stddev']}")
    print(f"first:     {s['first']}")
    print(f"last:      {s['last']}")


def markdown_stats_table(title, stats, percent=False):
    def fmt(v):
        if v is None:
            return "N/A"
        if percent:
            return f"{v * 100.0:.6f}%"
        return f"{v:.9g}"

    lines = []
    lines.append(f"## {title}")
    lines.append("")
    lines.append("| Statistic | Value |")
    lines.append("|-----------|------:|")
    lines.append(f"| Count | {stats['count']} |")
    lines.append(f"| Finite | {stats['finite']} |")
    lines.append(f"| Minimum | {fmt(stats['min'])} |")
    lines.append(f"| Maximum | {fmt(stats['max'])} |")
    lines.append(f"| Mean | {fmt(stats['mean'])} |")
    lines.append(f"| Std Dev | {fmt(stats['stddev'])} |")
    lines.append(f"| First | {fmt(stats['first'])} |")
    lines.append(f"| Last | {fmt(stats['last'])} |")
    lines.append("")
    return "\n".join(lines)


def write_markdown_stats(
    outfile,
    expr,
    n,
    tolerance,
    seed,
    xstart,
    xstep,
    x_values,
    base_values,
    output_values,
    command_line,
):
    outpath = Path(outfile)
    stats_file = outpath.with_name(outpath.stem + "_stats.md")

    x_stats = stats_dict(x_values)
    base_stats = stats_dict(base_values)
    output_stats = stats_dict(output_values)

    noise_values = output_values - base_values
    noise_stats = stats_dict(noise_values)

    pct_noise = noise_values / np.maximum(np.abs(base_values), 1.0)
    pct_noise_stats = stats_dict(pct_noise)

    preview_count = min(10, n)

    with open(stats_file, "w", encoding="utf-8") as f:
        f.write("# Dataset Statistics\n\n")

        f.write("## Dataset\n\n")
        f.write("| Item | Value |\n")
        f.write("|------|------:|\n")
        f.write(f"| Expression | `{expr}` |\n")
        f.write(f"| Samples | {n} |\n")
        f.write(f"| x start | {xstart:.9g} |\n")
        f.write(f"| x step | {xstep:.9g} |\n")
        f.write(f"| Output file | `{outfile}` |\n")
        f.write("| Data type | float32 |\n")
        f.write("| Byte order | Little Endian |\n")
        f.write(f"| Noise tolerance | ±{tolerance * 100.0:.6f}% |\n")
        f.write(f"| Random seed | {seed if seed is not None else 'None'} |\n")
        f.write("\n")

        f.write(markdown_stats_table("X Value Statistics", x_stats))
        f.write(markdown_stats_table("Base Expression Statistics", base_stats))
        f.write(markdown_stats_table("Output Dataset Statistics", output_stats))

        if tolerance > 0.0:
            f.write(markdown_stats_table("Absolute Noise Statistics", noise_stats))
            f.write(markdown_stats_table("Percentage Noise Statistics", pct_noise_stats, percent=True))

        f.write("## Preview\n\n")
        f.write("| Sample | x | Base | Output | Noise | Noise % |\n")
        f.write("|------:|--:|-----:|-------:|------:|--------:|\n")

        for i in range(preview_count):
            x = float(x_values[i])
            base = float(base_values[i])
            out = float(output_values[i])
            noise = out - base
            pct = noise / max(abs(base), 1.0) * 100.0

            f.write(
                f"| {i} | {x:.9g} | {base:.9g} | {out:.9g} | {noise:.9g} | {pct:.6f}% |\n"
            )

        f.write("\n")

        f.write("## Generation Command\n\n")
        f.write("```text\n")
        f.write(command_line)
        f.write("\n```\n")

    return str(stats_file)


def interactive_plot(x_values, base_values, output_values, expr, tolerance):
    n = len(output_values)

    zoom_levels = []
    z = 10
    while z < n:
        zoom_levels.append(z)
        z *= 10
    zoom_levels.append(n)

    zoom_index = zoom_levels.index(1000) if 1000 in zoom_levels else 0
    start = 0

    fig, ax = plt.subplots()

    base_line, = ax.plot(
        [],
        [],
        color="blue",
        marker=".",
        linestyle="-",
        label="Base expression",
    )

    noisy_line, = ax.plot(
        [],
        [],
        color="red",
        marker=".",
        linestyle="-",
        label="Noisy output",
    )

    def current_window():
        return zoom_levels[zoom_index]

    def update_plot():
        nonlocal start

        window = current_window()

        if window >= n:
            start = 0
            end = n
        else:
            start = max(0, min(start, n - window))
            end = start + window

        xs = x_values[start:end]
        base_ys = base_values[start:end]
        output_ys = output_values[start:end]

        base_line.set_data(xs, base_ys)

        if tolerance > 0.0:
            noisy_line.set_data(xs, output_ys)
            noisy_line.set_visible(True)
        else:
            noisy_line.set_data([], [])
            noisy_line.set_visible(False)

        xmin = float(xs[0])
        xmax = float(xs[-1])
        if xmin == xmax:
            xmin -= 1.0
            xmax += 1.0
        ax.set_xlim(xmin, xmax)

        visible_values = base_ys
        if tolerance > 0.0:
            visible_values = np.concatenate((base_ys, output_ys))

        finite = visible_values[np.isfinite(visible_values)]

        if len(finite) > 0:
            ymin = float(np.min(finite))
            ymax = float(np.max(finite))

            if ymin == ymax:
                ymin -= 1.0
                ymax += 1.0

            pad = 0.05 * (ymax - ymin)
            ax.set_ylim(ymin - pad, ymax + pad)

        title = f"y = {expr}    samples={start}..{end - 1}    window={window}"

        if tolerance > 0.0:
            title += f"    noise=±{tolerance * 100.0:.3f}%"

        ax.set_title(title)
        ax.set_xlabel("x value")
        ax.set_ylabel("y value")
        ax.grid(True)
        ax.legend()

        fig.canvas.draw_idle()

    def on_key(event):
        nonlocal start, zoom_index

        window = current_window()

        if event.key == "right":
            start += window
        elif event.key == "left":
            start -= window
        elif event.key == "up":
            if zoom_index < len(zoom_levels) - 1:
                center = start + window // 2
                zoom_index += 1
                start = center - current_window() // 2
        elif event.key == "down":
            if zoom_index > 0:
                center = start + window // 2
                zoom_index -= 1
                start = center - current_window() // 2
        else:
            return

        update_plot()

    fig.canvas.mpl_connect("key_press_event", on_key)

    print("\nPlot controls")
    print("-------------")
    print("Left/right arrows: page backward/forward")
    print("Up arrow:          zoom out")
    print("Down arrow:        zoom in")
    print("Zoom levels:       10, 100, 1000, 10000, ... all")

    update_plot()
    plt.show()


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Generate raw little-endian float32 data from a mathematical "
            "expression y = f(x)."
        ),
        epilog=(
            "Use --guide for detailed explanations, domain notes, use cases, "
            "and examples."
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    parser.add_argument("expr", help='Expression using x, e.g. "3*x+5"')
    parser.add_argument("n", type=int, help="Number of float32 values")
    parser.add_argument("outfile", help="Output binary .f32 file")

    parser.add_argument(
        "--xstart",
        type=float,
        default=0.0,
        help="Starting x value. Default: 0.0",
    )

    parser.add_argument(
        "--xstep",
        type=float,
        default=1.0,
        help="Step between x values. Default: 1.0",
    )

    parser.add_argument(
        "--tolerance",
        type=float,
        default=0.0,
        help="Fractional noise tolerance. Example: 0.025 means ±2.5%% noise.",
    )

    parser.add_argument(
        "--seed",
        type=int,
        default=None,
        help="Random seed for reproducible noise.",
    )

    parser.add_argument(
        "--stats",
        action="store_true",
        help="Print statistics to console.",
    )

    parser.add_argument(
        "--plot",
        action="store_true",
        help="Open interactive plot viewer.",
    )

    parser.add_argument(
        "--guide",
        action="version",
        version=GUIDE_TEXT,
        help="Print the complete usage guide and exit.",
    )

    args = parser.parse_args()

    if args.n <= 0:
        parser.error("N must be a positive integer")

    if args.xstep == 0.0:
        parser.error("--xstep must not be zero")

    if args.tolerance < 0.0:
        parser.error("--tolerance must be non-negative")

    if args.seed is not None:
        random.seed(args.seed)

    try:
        x_values, base_values, output_values = generate_values(
            args.expr,
            args.n,
            args.tolerance,
            args.xstart,
            args.xstep,
        )
    except (ValueError, RuntimeError) as exc:
        parser.exit(1, f"error: {exc}\n")

    write_f32_le(args.outfile, output_values)

    stats_file = write_markdown_stats(
        args.outfile,
        args.expr,
        args.n,
        args.tolerance,
        args.seed,
        args.xstart,
        args.xstep,
        x_values,
        base_values,
        output_values,
        " ".join(sys.argv),
    )

    print(f"Wrote {args.n} little-endian float32 values to {args.outfile}")
    print(f"Wrote stats markdown to {stats_file}")
    print(f"x range: {float(x_values[0]):.9g} to {float(x_values[-1]):.9g}")
    print(f"x step:  {args.xstep:.9g}")

    if args.tolerance > 0.0:
        print(f"Applied fractional noise tolerance: ±{args.tolerance * 100.0:.3f}%")

    if args.stats:
        print_stats(x_values, "X values")
        print_stats(base_values, "Base expression")
        print_stats(output_values, "Output dataset")

        if args.tolerance > 0.0:
            print_stats(output_values - base_values, "Noise")

    if args.plot:
        interactive_plot(x_values, base_values, output_values, args.expr, args.tolerance)


if __name__ == "__main__":
    main()

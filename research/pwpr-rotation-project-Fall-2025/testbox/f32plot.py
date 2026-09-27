#!/usr/bin/env python3
"""
plot_f32_paged.py

Plot one or more little-endian binary f32 files using matplotlib.

- X axis: index into the NumPy array
- Y axis: value at that index
- Shows N points per page (default: 1000) with keyboard paging
- Supports log-scale on x and/or y
- Can zoom out to show all data
- Multiple files plotted on the same axes with legend
- Prints basic statistics (min, max, mean, std, median) for each file
- Uses global min/max to keep y-axis consistent across all pages

Keyboard controls:
    n / right arrow : next page
    p / left arrow  : previous page
    h               : go to first page
    a               : show ALL data
    ?               : show help window (CLI + keys)
    s               : show stats windows (one per file)
    q               : quit
"""

import argparse
import math
import os
import sys

import numpy as np
import matplotlib.pyplot as plt


def load_f32_le(path: str) -> np.ndarray:
    """Load a little-endian f32 binary file into a NumPy array."""
    try:
        data = np.fromfile(path, dtype='<f4')  # little-endian float32
    except OSError as e:
        print(f"Error reading {path}: {e}", file=sys.stderr)
        sys.exit(1)
    return data


def compute_stats(data_list, filenames):
    """Compute basic statistics for each file; return list of dicts."""
    stats = []
    for data, name in zip(data_list, filenames):
        if data.size == 0:
            stats.append(
                {
                    "filename": name,
                    "empty": True,
                    "count": 0,
                    "min": None,
                    "max": None,
                    "mean": None,
                    "std": None,
                    "median": None,
                }
            )
            continue
        stats.append(
            {
                "filename": name,
                "empty": False,
                "count": int(data.size),
                "min": float(np.min(data)),
                "max": float(np.max(data)),
                "mean": float(np.mean(data)),
                "std": float(np.std(data)),
                "median": float(np.median(data)),
            }
        )
    return stats


def print_stats(stats):
    """Print statistics for each file to stdout."""
    print("=== File statistics ===")
    for st in stats:
        name = st["filename"]
        if st["empty"]:
            print(f"{name}: EMPTY")
            continue
        print(f"{name}:")
        print(f"  count  = {st['count']}")
        print(f"  min    = {st['min']:.3f}")
        print(f"  max    = {st['max']:.3f}")
        print(f"  mean   = {st['mean']:.3f}")
        print(f"  std    = {st['std']:.3f}")
        print(f"  median = {st['median']:.3f}")
        print("=======================")


def show_help_window():
    """Open a separate matplotlib window showing CLI options and keys."""
    help_text = r"""
plot_f32_paged.py  -  Help

Command-line options:
  files             Input .f32 binary files (little-endian float32)
  --page-size, -p   Points per page in paged view (default: 1000)
  --view            'paged' (default) or 'all'
  --logx            Use logarithmic x-axis
  --logy            Use logarithmic y-axis
  --xlabel          X-axis label (default: "Index")
  --ylabel          Y-axis label (default: "Value")
  --title           Plot title (default: "f32 plot")

Keyboard controls:
  n / →             Next page
  p / ←             Previous page
  h                 First page (home)
  a                 Show ALL data
  s                 Show stats windows (one per file)
  ?                 Show this help window
  q                 Quit
"""

    fig, ax = plt.subplots()
    fig.canvas.manager.set_window_title("plot_f32_paged.py - Help")
    ax.axis("off")
    ax.text(
        0.01,
        0.99,
        help_text,
        va="top",
        ha="left",
        family="monospace",
        fontsize=9,
    )
    fig.tight_layout()
    fig.show()


def show_stats_windows(stats):
    """Open one window per file showing its stats."""
    for st in stats:
        name = os.path.basename(st["filename"])
        if st["empty"]:
            text = f"{name}\n\nEMPTY FILE (no data)"
        else:
            text = (
                f"{name}\n\n"
                f"count  = {st['count']}\n"
                f"min    = {st['min']}\n"
                f"max    = {st['max']}\n"
                f"mean   = {st['mean']}\n"
                f"std    = {st['std']}\n"
                f"median = {st['median']}\n"
            )

        fig, ax = plt.subplots()
        fig.canvas.manager.set_window_title(f"Stats - {name}")
        ax.axis("off")
        ax.text(
            0.01,
            0.99,
            text,
            va="top",
            ha="left",
            family="monospace",
            fontsize=9,
        )
        fig.tight_layout()
        fig.show()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Plot little-endian f32 binary files as index vs value."
    )
    parser.add_argument(
        "files",
        nargs="+",
        help="Input .f32 binary files (little-endian float32).",
    )
    parser.add_argument(
        "--page-size",
        "-p",
        type=int,
        default=1000,
        help="Number of points per page in paged view (default: 1000).",
    )
    parser.add_argument(
        "--view",
        choices=["paged", "all"],
        default="paged",
        help="Initial view mode: 'paged' (default) or 'all' (show all data).",
    )
    parser.add_argument(
        "--logx",
        action="store_true",
        help="Use logarithmic scale for the x-axis.",
    )
    parser.add_argument(
        "--logy",
        action="store_true",
        help="Use logarithmic scale for the y-axis.",
    )
    parser.add_argument(
        "--xlabel",
        default="Index",
        help="X-axis label (default: 'Index').",
    )
    parser.add_argument(
        "--ylabel",
        default="Value",
        help="Y-axis label (default: 'Value').",
    )
    parser.add_argument(
        "--title",
        default="f32 plot",
        help="Plot title.",
    )
    return parser.parse_args()


def plot_all(
    ax,
    data_list,
    filenames,
    logx,
    logy,
    xlabel,
    ylabel,
    title,
    y_min,
    y_max,
    y_min_pos=None,
    y_max_pos=None,
):
    """Plot all data from all files on one axes."""
    ax.clear()

    for data, name in zip(data_list, filenames):
        if len(data) == 0:
            continue
        if logx:
            x = np.arange(1, len(data) + 1, dtype=float)
        else:
            x = np.arange(len(data), dtype=float)
        y = data
        ax.plot(x, y, label=os.path.basename(name))

    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.set_title(title + " (all data)")

    if logx:
        ax.set_xscale("log")
    if logy:
        if y_min_pos is None or y_max_pos is None:
            print(
                "Warning: logy requested but no positive y-values found.",
                file=sys.stderr,
            )
        else:
            ax.set_yscale("log")
            ax.set_ylim(y_min_pos, y_max_pos)
    else:
        ax.set_ylim(y_min, y_max)

    # Legend fixed on left side (inside the axes)
    ax.legend(loc="upper left")
    ax.grid(True)


def plot_page(
    ax,
    data_list,
    filenames,
    page,
    page_size,
    logx,
    logy,
    xlabel,
    ylabel,
    title,
    y_min,
    y_max,
    y_min_pos=None,
    y_max_pos=None,
):
    """Plot a single page of data (same index range) from all files."""
    ax.clear()

    max_len = max(len(d) for d in data_list)
    num_pages = max(1, math.ceil(max_len / page_size))
    page = max(0, min(page, num_pages - 1))

    start = page * page_size
    end = min(start + page_size, max_len)

    for data, name in zip(data_list, filenames):
        if len(data) == 0 or start >= len(data):
            continue
        local_end = min(end, len(data))
        if logx:
            x = np.arange(start + 1, local_end + 1, dtype=float)
        else:
            x = np.arange(start, local_end, dtype=float)
        y = data[start:local_end]
        ax.plot(x, y, label=os.path.basename(name))

    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.set_title(
        f"{title} (page {page + 1}/{num_pages}, indices {start}–{end - 1})"
    )

    if logx:
        ax.set_xscale("log")
    if logy:
        if y_min_pos is None or y_max_pos is None:
            print(
                "Warning: logy requested but no positive y-values found.",
                file=sys.stderr,
            )
        else:
            ax.set_yscale("log")
            ax.set_ylim(y_min_pos, y_max_pos)
    else:
        ax.set_ylim(y_min, y_max)

    ax.legend(loc="upper left")
    ax.grid(True)

    return page, num_pages


def main():
    args = parse_args()

    # Load all files
    data_list = [load_f32_le(f) for f in args.files]
    if not data_list:
        print("No data loaded.", file=sys.stderr)
        sys.exit(1)

    max_len = max(len(d) for d in data_list)
    if max_len == 0:
        print("All input files are empty.", file=sys.stderr)
        sys.exit(1)

    # Compute and print statistics
    stats = compute_stats(data_list, args.files)
    print_stats(stats)

    # Global y-limits across all files
    y_min = min(st["min"] for st in stats if not st["empty"])
    y_max = max(st["max"] for st in stats if not st["empty"])

    # For log-y, we need global positive min/max
    y_min_pos = None
    y_max_pos = None
    if args.logy:
        positive_vals = [d[d > 0] for d in data_list if np.any(d > 0)]
        if positive_vals:
            pos_concat = np.concatenate(positive_vals)
            y_min_pos = float(np.min(pos_concat))
            y_max_pos = float(np.max(pos_concat))
        else:
            print(
                "Warning: No positive values found for log-y scale.",
                file=sys.stderr,
            )

    fig, ax = plt.subplots()
    current_page = 0
    num_pages = max(1, math.ceil(max_len / args.page_size))

    if args.view == "all":
        plot_all(
            ax,
            data_list,
            args.files,
            args.logx,
            args.logy,
            args.xlabel,
            args.ylabel,
            args.title,
            y_min,
            y_max,
            y_min_pos,
            y_max_pos,
        )
    else:
        current_page, num_pages = plot_page(
            ax,
            data_list,
            args.files,
            current_page,
            args.page_size,
            args.logx,
            args.logy,
            args.xlabel,
            args.ylabel,
            args.title,
            y_min,
            y_max,
            y_min_pos,
            y_max_pos,
        )

    # Key handler for paging, zoom, help, stats
    def on_key(event):
        nonlocal current_page
        if event.key in ("n", "right"):
            current_page = min(current_page + 1, num_pages - 1)
            plot_page(
                ax,
                data_list,
                args.files,
                current_page,
                args.page_size,
                args.logx,
                args.logy,
                args.xlabel,
                args.ylabel,
                args.title,
                y_min,
                y_max,
                y_min_pos,
                y_max_pos,
            )
            fig.canvas.draw_idle()
        elif event.key in ("p", "left"):
            current_page = max(current_page - 1, 0)
            plot_page(
                ax,
                data_list,
                args.files,
                current_page,
                args.page_size,
                args.logx,
                args.logy,
                args.xlabel,
                args.ylabel,
                args.title,
                y_min,
                y_max,
                y_min_pos,
                y_max_pos,
            )
            fig.canvas.draw_idle()
        elif event.key == "h":  # home
            current_page = 0
            plot_page(
                ax,
                data_list,
                args.files,
                current_page,
                args.page_size,
                args.logx,
                args.logy,
                args.xlabel,
                args.ylabel,
                args.title,
                y_min,
                y_max,
                y_min_pos,
                y_max_pos,
            )
            fig.canvas.draw_idle()
        elif event.key == "a":  # show all
            plot_all(
                ax,
                data_list,
                args.files,
                args.logx,
                args.logy,
                args.xlabel,
                args.ylabel,
                args.title,
                y_min,
                y_max,
                y_min_pos,
                y_max_pos,
            )
            fig.canvas.draw_idle()
        elif event.key == "?":  # help window
            show_help_window()
        elif event.key == "s":  # stats windows
            show_stats_windows(stats)
        elif event.key == "q":
            plt.close(fig)

    fig.canvas.mpl_connect("key_press_event", on_key)
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()

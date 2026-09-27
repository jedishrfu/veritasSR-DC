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
    right / left     : next / previous page
    up / down        : zoom in / out
    [ / ]            : select previous / next comparison file
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
  → / ←             Next / previous page
  ↑ / ↓             Zoom in / out (fewer / more points per page)
  [ / ]             Select previous / next comparison file
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


def find_compressed_artifact(reconstructed_path):
    """Return the compressed file associated with a reconstructed .f32 file."""
    stem, _ = os.path.splitext(reconstructed_path)
    candidates = [stem + ext for ext in (".zsr", ".pfpl", ".sz3", ".zfp")]

    # Compression tools use different reconstructed-file suffixes.
    suffix_pairs = (
        ("_pfpl_new", ".pfpl"),
        ("_sz3_new", ".sz3"),
        ("_new", ".zfp"),
    )
    for suffix, extension in suffix_pairs:
        if stem.endswith(suffix):
            candidates.append(stem[:-len(suffix)] + extension)

    # Support Exprgen files created before the uniform _genN naming change.
    marker = "_new_gen"
    marker_position = stem.rfind(marker)
    if marker_position != -1:
        candidates.append(
            stem[:marker_position] + "_gen" +
            stem[marker_position + len(marker):] + ".zsr"
        )

    for candidate in candidates:
        if os.path.isfile(candidate):
            return candidate
    return None


def compression_text(data_list, filenames, selected_index):
    """Compute error and compression metrics relative to the first file."""
    if len(filenames) < 2 or selected_index is None:
        return "Compression: add a second file to enable comparison"

    sample_count = min(len(data_list[0]), len(data_list[selected_index]))
    if sample_count == 0:
        return "Compression metrics unavailable: one of the compared files is empty"

    # Only count the reference samples represented by the reconstruction.
    reference_size = sample_count * data_list[0].dtype.itemsize
    compressed_path = find_compressed_artifact(filenames[selected_index])

    reference = data_list[0][:sample_count].astype(np.float64)
    selected = data_list[selected_index][:sample_count].astype(np.float64)
    finite = np.isfinite(reference) & np.isfinite(selected)
    if not np.any(finite):
        return "Compression metrics unavailable: no finite sample pairs"

    reference = reference[finite]
    selected = selected[finite]
    error = selected - reference
    abs_error = np.abs(error)
    max_abs_error = float(np.max(abs_error))
    mse = float(np.mean(error ** 2))
    mae = float(np.mean(abs_error))
    signal_range = float(np.max(reference) - np.min(reference))
    if mse == 0.0:
        psnr = float("inf")
    elif signal_range == 0.0:
        psnr = float("-inf")
    else:
        psnr = 20.0 * math.log10(signal_range / math.sqrt(mse))

    psnr_text = "∞" if math.isinf(psnr) and psnr > 0 else (
        "-∞" if math.isinf(psnr) else f"{psnr:.4g} dB"
    )
    if compressed_path is None:
        cr_text = "unavailable (compressed file not found)"
    else:
        compressed_size = os.path.getsize(compressed_path)
        cr = reference_size / compressed_size if compressed_size else float("inf")
        cr_value = "∞" if math.isinf(cr) else f"{cr:.4g}x"
        cr_text = f"{cr_value} ({os.path.basename(compressed_path)})"
    return (
        f"MaxAbsErr {max_abs_error:.6g}  |  MSE {mse:.6g}  |  "
        f"MAE {mae:.6g}  |  PSNR {psnr_text}  |  CR {cr_text}"
    )


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
    selected_index=None,
):
    """Plot all data from all files on one axes."""
    ax.clear()

    # Draw the selected file first and the blue reference last so the
    # reference cannot be obscured by another series.
    plot_order = ([selected_index] if selected_index is not None else []) + [0]
    for index in plot_order:
        data = data_list[index]
        name = filenames[index]
        if len(data) == 0:
            continue
        if logx:
            x = np.arange(1, len(data) + 1, dtype=float)
        else:
            x = np.arange(len(data), dtype=float)
        y = data
        color = "blue" if index == 0 else "red"
        ax.plot(x, y, color=color, label=os.path.basename(name))

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

    ax.legend(
        loc="upper center", bbox_to_anchor=(0.5, -0.24),
        borderaxespad=0, ncol=2,
    )
    ax.grid(True)
    ax.text(
        0.5, -0.17, compression_text(data_list, filenames, selected_index),
        transform=ax.transAxes, ha="center", va="top", fontsize=9,
    )


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
    selected_index=None,
):
    """Plot a single page of data (same index range) from all files."""
    ax.clear()

    max_len = max(len(d) for d in data_list)
    num_pages = max(1, math.ceil(max_len / page_size))
    page = max(0, min(page, num_pages - 1))

    start = page * page_size
    end = min(start + page_size, max_len)

    # Draw the selected file first and the blue reference last so the
    # reference cannot be obscured by another series.
    plot_order = ([selected_index] if selected_index is not None else []) + [0]
    for index in plot_order:
        data = data_list[index]
        name = filenames[index]
        if len(data) == 0 or start >= len(data):
            continue
        local_end = min(end, len(data))
        if logx:
            x = np.arange(start + 1, local_end + 1, dtype=float)
        else:
            x = np.arange(start, local_end, dtype=float)
        y = data[start:local_end]
        color = "blue" if index == 0 else "red"
        ax.plot(x, y, color=color, label=os.path.basename(name))

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

    ax.legend(
        loc="upper center", bbox_to_anchor=(0.5, -0.24),
        borderaxespad=0, ncol=2,
    )
    ax.grid(True)
    ax.text(
        0.5, -0.17, compression_text(data_list, filenames, selected_index),
        transform=ax.transAxes, ha="center", va="top", fontsize=9,
    )

    return page, num_pages


def main():
    args = parse_args()
    if args.page_size < 2:
        print("--page-size must be at least 2.", file=sys.stderr)
        sys.exit(2)

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
    page_size = min(args.page_size, max_len)
    num_pages = max(1, math.ceil(max_len / page_size))
    selected_index = 1 if len(args.files) > 1 else None

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
            selected_index,
        )
    else:
        current_page, num_pages = plot_page(
            ax,
            data_list,
            args.files,
            current_page,
            page_size,
            args.logx,
            args.logy,
            args.xlabel,
            args.ylabel,
            args.title,
            y_min,
            y_max,
            y_min_pos,
            y_max_pos,
            selected_index,
        )

    # Avoid matplotlib's toolbar consuming the paging keys too.
    for keymap_name in ("keymap.back", "keymap.forward"):
        plt.rcParams[keymap_name] = [
            key for key in plt.rcParams[keymap_name] if key not in ("left", "right")
        ]

    def redraw():
        nonlocal current_page, num_pages
        current_page, num_pages = plot_page(
            ax, data_list, args.files, current_page, page_size,
            args.logx, args.logy, args.xlabel, args.ylabel, args.title,
            y_min, y_max, y_min_pos, y_max_pos, selected_index,
        )
        fig.canvas.draw_idle()

    # Key handler for paging, zoom, selection, help, and stats.
    def on_key(event):
        nonlocal current_page, page_size, selected_index
        if event.key in ("n", "right"):
            current_page = min(current_page + 1, num_pages - 1)
            redraw()
        elif event.key in ("p", "left"):
            current_page = max(current_page - 1, 0)
            redraw()
        elif event.key in ("up", "down"):
            center = current_page * page_size + page_size / 2.0
            if event.key == "up":
                page_size = max(2, math.ceil(page_size / 2))
            else:
                page_size = min(max_len, page_size * 2)
            current_page = max(0, int(center // page_size))
            redraw()
        elif event.key in ("[", "]") and len(args.files) > 1:
            offset = -1 if event.key == "[" else 1
            selected_index = 1 + ((selected_index - 1 + offset) % (len(args.files) - 1))
            redraw()
        elif event.key == "h":  # home
            current_page = 0
            redraw()
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
                selected_index,
            )
            fig.canvas.draw_idle()
        elif event.key == "?":  # help window
            show_help_window()
        elif event.key == "s":  # stats windows
            show_stats_windows(stats)
        elif event.key == "q":
            plt.close(fig)

    fig.canvas.mpl_connect("key_press_event", on_key)
    fig.subplots_adjust(bottom=0.30)
    plt.show()


if __name__ == "__main__":
    main()

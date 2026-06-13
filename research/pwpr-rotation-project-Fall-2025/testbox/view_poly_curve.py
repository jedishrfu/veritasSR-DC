#!/usr/bin/env python3
"""
Interactive plot of polynomial segments over original data, using the
same reconstruction formula as the C++ pwprDecompress().

Inputs:
    1) data_file:     binary float32 array of original data (y-values)
                      assumed indexed by x = 0, 1, 2, ..., N-1
    2) segments_file: binary polynomial segments file

Segments file layout per segment (little-endian):

    uint32  body_size_prefix   (4 bytes, may be 40 or 44 in legacy files)
    uint64  start              (8 bytes)  // inclusive
    uint64  end                (8 bytes)  // exclusive
    uint32  degree             (4 bytes)  // metadata
    float32 coeffs[5]          (20 bytes)

Reconstruction matches your C++:

    len   = end - start
    denom = (len > 1) ? (len - 1) : 1.0
    for i in [start, end):
        xlocal = i - start
        xprime = xlocal / denom            # normalized to [0,1]
        sum = c0 + c1*xprime + c2*xprime^2 + c3*xprime^3 + c4*xprime^4
        yhat[i] = sum

Plot:
  - Draw original data as a light gray background curve.
  - Overlay each segment colored by degree.

Keyboard controls (click on the plot window first):
    1,2,3,4 : toggle highlight for that degree (force those segments to blue)
    0       : toggle visibility of ALL highlighted (blue) segments
    n       : move x-window forward by 1000 units
    p       : move x-window backward by 1000 units
    a       : show the entire graph

Legend:
  - Always sorted: original data, then degree 1..4
  - Degree labels include counts and percentages: "degree d (N=..., ...%)"
"""

import argparse
import struct
from dataclasses import dataclass
from typing import List, Tuple
from collections import Counter

import numpy as np
import matplotlib.pyplot as plt


@dataclass
class Segment:
    start: int
    end: int   # exclusive
    degree: int
    coeffs: Tuple[float, float, float, float, float]


def read_segments(filename: str) -> List[Segment]:
    """
    Read all segments from the given binary file.

    Accepts both:
      - body_size = 40  (correct)
      - body_size = 44  (legacy: rec_size mistakenly 4 bytes larger)

    In all cases, only 40 bytes of body are actually read.
    """
    segments: List[Segment] = []

    prefix_fmt = "<I"         # uint32_t record_size prefix
    body_fmt   = "<QQI5f"     # uint64_t start, uint64_t end, uint32_t degree, 5 floats

    prefix_size = struct.calcsize(prefix_fmt)  # 4
    body_size   = struct.calcsize(body_fmt)    # 40

    recnum = 0

    with open(filename, "rb") as f:
        while True:
            prefix = f.read(prefix_size)
            if not prefix:
                break  # clean EOF
            if len(prefix) != prefix_size:
                raise ValueError(
                    f"Truncated record_size prefix at record #{recnum}: "
                    f"expected {prefix_size} bytes, got {len(prefix)}"
                )

            (sz,) = struct.unpack(prefix_fmt, prefix)

            # Accept correct 40 and legacy 44
            if sz not in (body_size, body_size + 4):
                raise ValueError(
                    f"Record size mismatch at record #{recnum}: "
                    f"file says {sz}, expected {body_size} or {body_size + 4}"
                )

            body = f.read(body_size)
            if len(body) != body_size:
                raise ValueError(
                    f"Truncated record body at record #{recnum}: "
                    f"expected {body_size}, got {len(body)}"
                )

            start_u64, end_u64, degree_u32, *coeffs = struct.unpack(body_fmt, body)

            segments.append(
                Segment(
                    start=int(start_u64),
                    end=int(end_u64),     # exclusive
                    degree=int(degree_u32),
                    coeffs=tuple(float(c) for c in coeffs),
                )
            )
            recnum += 1

    return segments


def eval_segment_like_cpp(coeffs: Tuple[float, ...],
                          x: np.ndarray,
                          start: int,
                          end: int) -> np.ndarray:
    """
    Evaluate segment polynomial using the same normalization as C++ pwprDecompress().

    len_seg = end - start
    denom   = (len_seg > 1) ? (len_seg - 1) : 1
    xprime  = (x - start) / denom

    y = sum_{j=0..4} coeff[j] * xprime^j
    """
    len_seg = max(end - start, 1)
    denom = float(len_seg - 1) if len_seg > 1 else 1.0
    xprime = (x - float(start)) / denom

    y = np.zeros_like(xprime, dtype=float)
    p = np.ones_like(xprime, dtype=float)
    for c in coeffs:
        y += c * p
        p *= xprime
    return y


def apply_sorted_degree_legend(ax,
                               deg_counts: Counter,
                               total_segments: int):
    """
    Replace the current legend with a sorted one:
      original data first, then degree 1..4
    and degree labels formatted as:
      "degree d (N=..., ...%)"
    """
    handles, labels = ax.get_legend_handles_labels()

    def sort_key(lbl: str):
        if lbl.startswith("degree "):
            try:
                d = int(lbl.split()[1])
                return (1, d)
            except Exception:
                return (1, 999)
        return (0, -1)

    pairs = sorted(zip(handles, labels), key=lambda p: sort_key(p[1]))

    new_handles = []
    new_labels = []

    for h, lbl in pairs:
        if lbl.startswith("degree "):
            d = int(lbl.split()[1])
            count = int(deg_counts.get(d, 0))
            pct = (100.0 * count / total_segments) if total_segments > 0 else 0.0
            new_labels.append(f"degree {d} (N={count}, {pct:.1f}%)")
            new_handles.append(h)
        else:
            new_labels.append(lbl)
            new_handles.append(h)

    ax.legend(new_handles, new_labels)


def plot_segments_interactive(y_data: np.ndarray,
                              segments: List[Segment],
                              deg_counts: Counter,
                              title: str,
                              window_size: float = 1000.0):
    if y_data.size == 0:
        print("No data points to plot.")
        return
    if not segments:
        print("No segments to plot.")
        return

    total_segments = sum(deg_counts.values())

    x_data = np.arange(len(y_data), dtype=float)

    degree_colors = {
        0: "gray",
        1: "tab:blue",
        2: "tab:orange",
        3: "tab:green",
        4: "tab:red",
    }

    data_xmin, data_xmax = x_data[0], x_data[-1]
    seg_xmin = min(seg.start for seg in segments)
    seg_xmax = max(seg.end - 1 for seg in segments if seg.end > seg.start)
    global_xmin = float(min(data_xmin, seg_xmin))
    global_xmax = float(max(data_xmax, seg_xmax))

    fig, ax = plt.subplots(figsize=(10, 6))
    ax.set_title(title)
    ax.set_xlabel("x (index)")
    ax.set_ylabel("y")
    ax.grid(True)

    # Background raw data (light gray)
    ax.plot(
        x_data,
        y_data,
        color="0.8",
        linewidth=1.0,
        zorder=1,
        label="original data",
    )

    # We'll keep segment lines for toggling/hiding
    line_info = []
    used_degree_labels = set()

    for seg in segments:
        if seg.start >= seg.end:
            continue

        # Plot integer domain [start, end) but clamp to available data range
        x0 = max(seg.start, 0)
        x1 = min(seg.end - 1, len(y_data) - 1)
        if x0 > x1:
            continue

        num_samples = min(x1 - x0 + 1, 200)
        x_vals = np.linspace(x0, x1, num_samples, dtype=float)
        y_vals = eval_segment_like_cpp(seg.coeffs, x_vals, seg.start, seg.end)

        base_color = degree_colors.get(seg.degree, "black")

        # Use a simple label template for sorting; we’ll rewrite to include N/% later
        label = f"degree {seg.degree}"

        kwargs = dict(color=base_color, linewidth=2.0, alpha=0.9, zorder=3)
        if label in used_degree_labels:
            line, = ax.plot(x_vals, y_vals, **kwargs)
        else:
            line, = ax.plot(x_vals, y_vals, label=label, **kwargs)
            used_degree_labels.add(label)

        line_info.append((line, seg.degree, base_color))

    # Sorted legend with counts & percentages
    apply_sorted_degree_legend(ax, deg_counts=deg_counts, total_segments=total_segments)
    fig.tight_layout()

    # Interaction state
    highlighted_degrees = set()
    hide_blue = False
    current_xmin = global_xmin

    # Initial window: first 1000 units
    x_end_init = min(global_xmin + window_size, global_xmax)
    ax.set_xlim(global_xmin, x_end_init)

    print("Keyboard controls (click in the plot window first):")
    print("  1,2,3,4 : toggle highlight for that degree (blue / original color)")
    print("  0       : toggle visibility of ALL highlighted (blue) segments")
    print(f"  n       : move x-window forward by {window_size} units")
    print(f"  p       : move x-window backward by {window_size} units")
    print("  a       : show entire graph")
    print()

    def update_colors_and_visibility():
        for line, deg, base_color in line_info:
            if deg in highlighted_degrees:
                line.set_color("blue")
            else:
                line.set_color(base_color)

            if hide_blue and deg in highlighted_degrees:
                line.set_visible(False)
            else:
                line.set_visible(True)

    def on_key(event):
        nonlocal current_xmin, hide_blue

        if event.key in ("1", "2", "3", "4"):
            d = int(event.key)
            if d in highlighted_degrees:
                highlighted_degrees.remove(d)
                print(f"Degree {d}: un-highlight")
            else:
                highlighted_degrees.add(d)
                print(f"Degree {d}: highlight (blue)")
            update_colors_and_visibility()
            fig.canvas.draw_idle()

        elif event.key == "0":
            hide_blue = not hide_blue
            print("Highlighted (blue) segments are now", "hidden" if hide_blue else "visible")
            update_colors_and_visibility()
            fig.canvas.draw_idle()

        elif event.key == "n":
            new_xmin = current_xmin + window_size
            if new_xmin > global_xmax:
                new_xmin = global_xmax - window_size
            current_xmin = max(global_xmin, new_xmin)
            ax.set_xlim(current_xmin, current_xmin + window_size)
            print(f"Window: [{current_xmin}, {current_xmin + window_size}]")
            fig.canvas.draw_idle()

        elif event.key == "p":
            new_xmin = current_xmin - window_size
            if new_xmin < global_xmin:
                new_xmin = global_xmin
            current_xmin = new_xmin
            ax.set_xlim(current_xmin, current_xmin + window_size)
            print(f"Window: [{current_xmin}, {current_xmin + window_size}]")
            fig.canvas.draw_idle()

        elif event.key == "a":
            current_xmin = global_xmin
            ax.set_xlim(global_xmin, global_xmax)
            print(f"Window: full range [{global_xmin}, {global_xmax}]")
            fig.canvas.draw_idle()

    fig.canvas.mpl_connect("key_press_event", on_key)
    plt.show()


def main():
    parser = argparse.ArgumentParser(
        description="Interactive plot: original data (f32) + segment overlay (C++-compatible)."
    )
    parser.add_argument("data_file", help="Binary float32 file containing original y-values")
    parser.add_argument("segments_file", help="Binary segments file")
    parser.add_argument("--title", default="Polynomial Segments over Data (C++-compatible)")
    parser.add_argument("--window", type=float, default=1000.0)
    args = parser.parse_args()

    y_data = np.fromfile(args.data_file, dtype=np.float32)
    print(f"Read {y_data.size} data points from {args.data_file}")

    segments = read_segments(args.segments_file)
    print(f"Read {len(segments)} segments from {args.segments_file}")

    deg_counts = Counter(seg.degree for seg in segments)
    print("Degree counts:", dict(deg_counts))

    plot_segments_interactive(
        y_data=y_data,
        segments=segments,
        deg_counts=deg_counts,
        title=args.title,
        window_size=args.window,
    )


if __name__ == "__main__":
    main()
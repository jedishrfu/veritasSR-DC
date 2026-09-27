#!/usr/bin/env python3
"""
Poly_Plot (macOS) — interactive Matplotlib PWPR-style viewer

What this does
- Generate synthetic data (sine/cosine/raw) with bounded noise ±eps
- Find extrema (local maxima/minima) in the noisy data
- Build piecewise polynomial segments that are:
    • symmetric about each extremum (as much as possible without overlap)
    • and between extrema segments, fill gaps using a greedy PWPR segmenter
- For each fixed segment window, select the “best degree” regression among allowed degrees:
    • prefer the lowest degree that satisfies max|error| <= eps
    • if none satisfy eps, choose the degree with smallest max error (tie -> lower degree)
- Plot paged (1000 points) or all, with full Matplotlib toolbar (pan/zoom/home/save)

UI
- Prev / Next / Show All
- Show toggles: All Seg, Deg 1..4 (visibility)
- Fit toggles: Deg 1..4 (whether that degree is allowed during segmentation)
- Settings: mode radio + text boxes (N, x_min, x_max, eps) + Generate

Run
  python3 poly_plot.py
"""

import warnings
from dataclasses import dataclass
from typing import Dict, List, Tuple, Optional

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.widgets import Button, CheckButtons, RadioButtons, TextBox

warnings.simplefilter("ignore", np.RankWarning)

DEG_COLORS = {1: "red", 2: "green", 3: "blue", 4: "black"}


# ----------------------------
# Data + segments
# ----------------------------

@dataclass
class Segment:
    start: int
    end: int  # end-exclusive
    deg: int
    coef: np.ndarray
    mu: float
    scale: float
    kind: str = "gap"  # "extrema" or "gap" for info


def generate_synthetic(n: int, x_min: float, x_max: float, mode: str, eps: float) -> Tuple[np.ndarray, np.ndarray]:
    n = max(2, int(n))
    x = np.linspace(x_min, x_max, n, dtype=np.float64)
    noise = np.random.uniform(-eps, eps, size=n).astype(np.float64)
    if mode == "sine":
        y = np.sin(x) + noise
    elif mode == "cosine":
        y = np.cos(x) + noise
    elif mode == "raw":
        y = noise
    else:
        raise ValueError(f"Unknown mode: {mode}")
    return x, y


def extrema_mask(y: np.ndarray) -> np.ndarray:
    y = np.asarray(y, dtype=np.float64)
    n = len(y)
    m = np.zeros(n, dtype=bool)
    if n < 3:
        return m
    prev = y[:-2]
    mid = y[1:-1]
    nxt = y[2:]
    m[1:-1] = ((mid > prev) & (mid > nxt)) | ((mid < prev) & (mid < nxt))
    return m


def _normalize_x(xs: np.ndarray) -> Tuple[np.ndarray, float, float]:
    mu = float(np.mean(xs))
    span = float(np.max(xs) - np.min(xs))
    scale = span if span > 0 else 1.0
    z = (xs - mu) / scale
    return z, mu, scale


def max_abs_err_for_fit(x: np.ndarray, y: np.ndarray, start: int, length: int, deg: int) -> Tuple[float, np.ndarray, float, float]:
    xs = x[start:start + length]
    ys = y[start:start + length]
    z, mu, scale = _normalize_x(xs)
    coef = np.polyfit(z, ys, deg)
    yhat = np.polyval(coef, z)
    err = float(np.max(np.abs(yhat - ys)))
    return err, coef, mu, scale


def best_degree_for_fixed_window(
    x: np.ndarray,
    y: np.ndarray,
    start: int,
    end_excl: int,
    eps: float,
    fit_degrees: List[int],
    kind: str
) -> Segment:
    """
    Pick best degree for fixed window [start, end_excl)
      - Prefer lowest degree that satisfies max|error| <= eps
      - Otherwise choose smallest max error (tie -> lower degree)
    """
    length = end_excl - start
    fit_degrees = sorted(fit_degrees) or [1]

    best = None  # (fits, err, deg, coef, mu, scale)
    for deg in fit_degrees:
        err, coef, mu, scale = max_abs_err_for_fit(x, y, start, length, deg)
        fits = (err <= eps)
        cand = (fits, err, deg, coef, mu, scale)

        if best is None:
            best = cand
            # if it already fits, since degrees are ascending, we're done
            if fits:
                break
            continue

        if cand[0] and not best[0]:
            best = cand
            break  # first fitting degree wins (lowest)
        elif (not cand[0]) and (not best[0]):
            if cand[1] < best[1] - 1e-15 or (abs(cand[1] - best[1]) <= 1e-15 and cand[2] < best[2]):
                best = cand

    fits, err, deg, coef, mu, scale = best
    return Segment(start, end_excl, deg, coef, mu, scale, kind=kind)


# ----------------------------
# Greedy gap segmenter (PWPR-style)
# ----------------------------

def fits_eps(x: np.ndarray, y: np.ndarray, start: int, length: int, deg: int, eps: float) -> Tuple[bool, Optional[np.ndarray], Optional[float], Optional[float]]:
    if length < deg + 1:
        return False, None, None, None
    xs = x[start:start + length]
    ys = y[start:start + length]
    try:
        z, mu, scale = _normalize_x(xs)
        coef = np.polyfit(z, ys, deg)
        yhat = np.polyval(coef, z)
        err = float(np.max(np.abs(yhat - ys)))
        return (err <= eps), coef, mu, scale
    except Exception:
        return False, None, None, None


def max_fit_len(x: np.ndarray, y: np.ndarray, start: int, deg: int, eps: float) -> Tuple[int, Optional[np.ndarray], Optional[float], Optional[float]]:
    n = len(x)
    lo = deg + 1
    hi = n - start
    if hi < lo:
        return 0, None, None, None

    ok, coef, mu, scale = fits_eps(x, y, start, lo, deg, eps)
    if not ok:
        return 0, None, None, None

    best_len, best_coef, best_mu, best_scale = lo, coef, mu, scale
    while lo <= hi:
        mid = (lo + hi) // 2
        ok, coef, mu, scale = fits_eps(x, y, start, mid, deg, eps)
        if ok:
            best_len, best_coef, best_mu, best_scale = mid, coef, mu, scale
            lo = mid + 1
        else:
            hi = mid - 1

    return best_len, best_coef, best_mu, best_scale


def segment_piecewise_greedy(x: np.ndarray, y: np.ndarray, eps: float, fit_degrees: List[int], offset: int, kind: str) -> List[Segment]:
    """
    Greedy PWPR segmenter on (x,y) arrays (already sliced). Offsets indices by 'offset'.
    Chooses degree that covers most points within eps (tie -> lower degree).
    """
    fit_degrees = sorted(fit_degrees) or [1]
    segs: List[Segment] = []
    i = 0
    n = len(x)

    while i < n - 1:
        bestL, bestD, bestC, bestMu, bestSc = 0, None, None, None, None
        for deg in fit_degrees:
            L, C, Mu, Sc = max_fit_len(x, y, i, deg, eps)
            if L > bestL or (L == bestL and L > 0 and deg < (bestD or 99)):
                bestL, bestD, bestC, bestMu, bestSc = L, deg, C, Mu, Sc

        if bestL < 2:
            bestL = min(2, n - i)
            xs = x[i:i + bestL]
            ys = y[i:i + bestL]
            z, mu, sc = _normalize_x(xs)
            bestD = 1
            bestC = np.polyfit(z, ys, 1)
            bestMu, bestSc = mu, sc

        segs.append(Segment(i + offset, i + offset + bestL, bestD, bestC, bestMu, bestSc, kind=kind))
        i += bestL

    return segs


# ----------------------------
# Symmetric-about-extrema segmentation
# ----------------------------

def build_symmetric_extrema_windows(n: int, extrema_idx: np.ndarray) -> List[Tuple[int, int, int]]:
    """
    Create symmetric windows [s, e_excl) about each extremum index k:
      [k-r, k+r] where r = min((k-kprev)//2, (knext-k)//2)
    Returns windows (s, e_excl, k). Windows may overlap; caller can resolve.
    """
    extrema_idx = np.asarray(extrema_idx, dtype=int)
    extrema_idx = extrema_idx[(extrema_idx > 0) & (extrema_idx < n - 1)]
    extrema_idx = np.unique(extrema_idx)
    if len(extrema_idx) == 0:
        return []

    ext = extrema_idx.tolist()
    windows: List[Tuple[int, int, int]] = []

    for j, k in enumerate(ext):
        kprev = 0 if j == 0 else ext[j - 1]
        knext = (n - 1) if j == len(ext) - 1 else ext[j + 1]
        r_left = (k - kprev) // 2
        r_right = (knext - k) // 2
        r = int(min(r_left, r_right))
        r = max(r, 1)  # at least 1 -> window >= 3 pts

        s = max(0, k - r)
        e = min(n - 1, k + r)
        windows.append((s, e + 1, k))

    windows.sort(key=lambda t: t[0])
    return windows


def resolve_nonoverlapping(windows: List[Tuple[int, int, int]], n: int) -> List[Tuple[int, int, int]]:
    """
    Turn possibly-overlapping symmetric windows into a non-overlapping set.
    We keep the window start order; if overlap occurs, we trim the later window's start to cursor.
    If trimming makes window too small (<3), we drop it.
    """
    out: List[Tuple[int, int, int]] = []
    cursor = 0
    for s, e, k in windows:
        if e <= cursor:
            continue
        if s < cursor:
            s = cursor
        if e - s < 3:
            continue
        out.append((s, e, k))
        cursor = e
    return out


def segment_centered_sym_extrema_full(
    x: np.ndarray,
    y: np.ndarray,
    eps: float,
    fit_degrees: List[int],
    extrema_idx: np.ndarray
) -> List[Segment]:
    """
    Full-domain segmentation:
      - Build (mostly) symmetric windows around extrema
      - Make them non-overlapping (trim later windows if needed)
      - Fill gaps between windows using greedy segmenter
      - For each extrema window, choose best degree for the fixed window
    """
    n = len(x)
    fit_degrees = sorted(fit_degrees) or [1]

    windows = build_symmetric_extrema_windows(n, extrema_idx)
    if not windows:
        # No extrema -> just greedy over all
        return segment_piecewise_greedy(x, y, eps, fit_degrees, offset=0, kind="gap")

    nonover = resolve_nonoverlapping(windows, n)

    segs: List[Segment] = []
    cursor = 0

    for s, e, k in nonover:
        # gap before
        if cursor < s:
            segs.extend(segment_piecewise_greedy(x[cursor:s], y[cursor:s], eps, fit_degrees, offset=cursor, kind="gap"))
        # extrema window
        segs.append(best_degree_for_fixed_window(x, y, s, e, eps, fit_degrees, kind="extrema"))
        cursor = e

    # tail
    if cursor < n:
        segs.extend(segment_piecewise_greedy(x[cursor:n], y[cursor:n], eps, fit_degrees, offset=cursor, kind="gap"))

    segs.sort(key=lambda s: s.start)
    return segs


# ----------------------------
# App
# ----------------------------

class PolyPlotApp:
    def __init__(self):
        # Settings
        self.n_points = 6000
        self.x_min = 0.0
        self.x_max = 40.0
        self.eps = 0.08
        self.mode = "sine"

        # Viewing
        self.page_len = 1000
        self.page_start = 0
        self.show_all = False

        # Display toggles
        self.show_all_segments = True
        self.show_deg: Dict[int, bool] = {1: True, 2: True, 3: True, 4: True}

        # Fit toggles (used in segmentation)
        self.fit_deg: Dict[int, bool] = {1: True, 2: True, 3: True, 4: True}

        # Data
        self.x = np.array([])
        self.y = np.array([])
        self.ext_m = np.array([], dtype=bool)
        self.segs: List[Segment] = []

        # Figure & axes
        self.fig = plt.figure("Poly_Plot", figsize=(12.5, 7.2))
        self.ax = self.fig.add_axes([0.07, 0.12, 0.62, 0.80])

        self.ax_info = self.fig.add_axes([0.71, 0.12, 0.27, 0.52])
        self.ax_info.axis("off")
        self.info_text = self.ax_info.text(0.0, 1.0, "", va="top", family="monospace")

        # Buttons
        self.btn_prev = Button(self.fig.add_axes([0.07, 0.03, 0.08, 0.05]), "Prev")
        self.btn_next = Button(self.fig.add_axes([0.16, 0.03, 0.08, 0.05]), "Next")
        self.btn_all = Button(self.fig.add_axes([0.25, 0.03, 0.10, 0.05]), "Show All")
        self.btn_prev.on_clicked(lambda _: self.on_prev())
        self.btn_next.on_clicked(lambda _: self.on_next())
        self.btn_all.on_clicked(lambda _: self.on_show_all())

        # Show toggles
        self.chk_show = CheckButtons(
            self.fig.add_axes([0.37, 0.02, 0.20, 0.11]),
            labels=["All Seg", "Deg 1", "Deg 2", "Deg 3", "Deg 4"],
            actives=[True, True, True, True, True],
        )
        self.chk_show.on_clicked(self.on_toggle_show)

        # Fit toggles label
        ax_fit_label = self.fig.add_axes([0.71, 0.66, 0.27, 0.04])
        ax_fit_label.axis("off")
        ax_fit_label.text(0.0, 0.2, "Fit degrees (run during segmentation):", family="monospace")

        # Fit toggles
        self.chk_fit = CheckButtons(
            self.fig.add_axes([0.71, 0.58, 0.27, 0.10]),
            labels=["Deg 1", "Deg 2", "Deg 3", "Deg 4"],
            actives=[True, True, True, True],
        )
        self.chk_fit.on_clicked(self.on_toggle_fit)

        # Mode radio
        self.radio_mode = RadioButtons(
            self.fig.add_axes([0.71, 0.86, 0.12, 0.12]),
            ("sine", "cosine", "raw"),
            active=0
        )
        self.radio_mode.on_clicked(self.on_mode)

        # Text boxes + generate
        self.tb_n = TextBox(self.fig.add_axes([0.85, 0.90, 0.12, 0.05]), "N", initial=str(self.n_points))
        self.tb_xmin = TextBox(self.fig.add_axes([0.85, 0.84, 0.12, 0.05]), "x_min", initial=str(self.x_min))
        self.tb_xmax = TextBox(self.fig.add_axes([0.85, 0.78, 0.12, 0.05]), "x_max", initial=str(self.x_max))
        self.tb_eps = TextBox(self.fig.add_axes([0.85, 0.72, 0.12, 0.05]), "eps", initial=str(self.eps))
        self.btn_gen = Button(self.fig.add_axes([0.85, 0.66, 0.12, 0.05]), "Generate")
        self.btn_gen.on_clicked(lambda _: self.on_generate())

        # Compute & draw
        self.recompute()
        self.redraw()

    def current_fit_degrees(self) -> List[int]:
        degs = [d for d in (1, 2, 3, 4) if self.fit_deg[d]]
        return degs if degs else [1]

    def recompute(self):
        self.x, self.y = generate_synthetic(self.n_points, self.x_min, self.x_max, self.mode, self.eps)
        self.ext_m = extrema_mask(self.y)
        ext_idx = np.where(self.ext_m)[0]
        self.segs = segment_centered_sym_extrema_full(self.x, self.y, self.eps, self.current_fit_degrees(), ext_idx)

    def window_indices(self) -> Tuple[int, int]:
        if self.show_all:
            return 0, len(self.x)
        i0 = max(0, min(self.page_start, len(self.x) - 1))
        i1 = min(i0 + self.page_len, len(self.x))
        return i0, i1

    def redraw(self):
        self.ax.clear()
        i0, i1 = self.window_indices()

        # stats
        mn = float(np.min(self.y))
        mx = float(np.max(self.y))
        med = float(np.median(self.y))
        std = float(np.std(self.y))

        fit_list = self.current_fit_degrees()
        self.ax.set_title(f"Poly_Plot — mode={self.mode}  eps=±{self.eps:g}  N={len(self.x)}  fit={fit_list}")

        self.ax.text(
            0.01, 1.02,
            f"min={mn:.4g}  max={mx:.4g}  median={med:.4g}  std={std:.4g}",
            transform=self.ax.transAxes
        )

        # original data (paged or all)
        self.ax.plot(self.x[i0:i1], self.y[i0:i1], color="0.7", alpha=0.35, lw=1.0)

        # overlay segments
        for s in self.segs:
            if s.end <= i0 or s.start >= i1:
                continue

            if self.show_all_segments:
                if not self.show_deg.get(s.deg, True):
                    continue
            else:
                if not self.show_deg.get(s.deg, False):
                    continue

            a = max(s.start, i0)
            b = min(s.end, i1)
            if b - a < 2:
                continue

            xs = self.x[a:b]
            z = (xs - s.mu) / s.scale
            yhat = np.polyval(s.coef, z)

            self.ax.plot(xs, yhat, color=DEG_COLORS[s.deg], lw=2.0, alpha=0.95)

        self.ax.set_xlabel("x")
        self.ax.set_ylabel("y")
        self.ax.grid(True, alpha=0.2)

        # info / legend
        ext_count = int(np.count_nonzero(self.ext_m))
        ext_pct = 100.0 * ext_count / max(1, len(self.y))

        counts = {d: 0 for d in (1, 2, 3, 4)}
        kinds = {"extrema": 0, "gap": 0}
        for s in self.segs:
            counts[s.deg] += 1
            kinds[s.kind] = kinds.get(s.kind, 0) + 1

        total_segs = max(1, len(self.segs))

        lines = []
        lines.append("Legend / Stats")
        lines.append("")
        lines.append("gray   data")
        lines.append(f"       extrema: {ext_count} ({ext_pct:5.2f}%)")
        lines.append("")
        for d in (1, 2, 3, 4):
            pct = 100.0 * counts[d] / total_segs
            cname = {1: "red", 2: "green", 3: "blue", 4: "black"}[d]
            lines.append(f"{cname:<5} deg {d}: segments {counts[d]:4d} ({pct:5.1f}%)")
        lines.append("")
        lines.append(f"Segments: {len(self.segs)}  (extrema-centered {kinds.get('extrema',0)}, gaps {kinds.get('gap',0)})")
        lines.append(f"Fit degrees enabled: {fit_list}")
        lines.append("View: ALL points" if self.show_all else f"View: points {i0}–{i1-1} (page {self.page_len})")

        self.info_text.set_text("\n".join(lines))
        self.fig.canvas.draw_idle()

    # ---------------- UI handlers ----------------

    def on_prev(self):
        self.show_all = False
        self.page_start = max(0, self.page_start - self.page_len)
        self.redraw()

    def on_next(self):
        self.show_all = False
        max_start = max(0, len(self.x) - self.page_len)
        self.page_start = min(max_start, self.page_start + self.page_len)
        self.redraw()

    def on_show_all(self):
        self.show_all = True
        self.redraw()

    def on_toggle_show(self, label: str):
        if label == "All Seg":
            self.show_all_segments = not self.show_all_segments
        else:
            d = int(label.split()[-1])
            self.show_deg[d] = not self.show_deg[d]
        self.redraw()

    def on_toggle_fit(self, label: str):
        d = int(label.split()[-1])
        self.fit_deg[d] = not self.fit_deg[d]
        if not any(self.fit_deg.values()):
            # keep at least degree 1 enabled logically
            self.fit_deg[1] = True
        # Re-segment (data unchanged)
        ext_idx = np.where(self.ext_m)[0]
        self.segs = segment_centered_sym_extrema_full(self.x, self.y, self.eps, self.current_fit_degrees(), ext_idx)
        self.redraw()

    def on_mode(self, mode: str):
        self.mode = mode

    def on_generate(self):
        def parse_int(tb: TextBox, fallback: int) -> int:
            try:
                return int(float(tb.text))
            except Exception:
                tb.set_val(str(fallback))
                return fallback

        def parse_float(tb: TextBox, fallback: float) -> float:
            try:
                return float(tb.text)
            except Exception:
                tb.set_val(str(fallback))
                return fallback

        n = parse_int(self.tb_n, self.n_points)
        xmin = parse_float(self.tb_xmin, self.x_min)
        xmax = parse_float(self.tb_xmax, self.x_max)
        eps = abs(parse_float(self.tb_eps, self.eps))

        if xmax == xmin:
            xmax = xmin + 1.0
        if xmax < xmin:
            xmin, xmax = xmax, xmin
        if eps <= 0:
            eps = 1e-6

        self.n_points, self.x_min, self.x_max, self.eps = n, xmin, xmax, eps
        self.page_start = 0
        self.show_all = False

        self.recompute()
        self.redraw()


if __name__ == "__main__":
    app = PolyPlotApp()
    plt.show()

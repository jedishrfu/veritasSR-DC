#!/usr/bin/env python3
"""
Compare a 3-edge AST against test data.

3-edge AST form used here:
    y = scalar * func(arg_scale * x) + offset

Edges:
    - argument edge  -> arg_scale * x
    - scalar edge    -> scalar
    - offset edge    -> offset

Behavior:
    - Generates one fixed TEST AST and 100 points of test data.
    - Plots test data in blue.
    - Generates one COMPUTED AST and plots it in red.
    - Prints the test AST, each computed AST, and MAE/MSE/RMSE to the console.
    - Press Enter in the plot window to generate another computed AST,
      plot it on the same figure, and print its stats.
    - Each computed curve is labeled 1, 2, 3, ... in runtime order.
"""

import math
import random
from dataclasses import dataclass
from typing import Callable, Dict, List

import matplotlib.pyplot as plt
import numpy as np


# ----------------------------
# Configuration
# ----------------------------

NUM_POINTS = 100
X_MIN = -10.0
X_MAX = 10.0
FUNCTION_NAME = "sin"   # Change to: "sin", "cos", "tanh", "exp_clip", "identity"


# ----------------------------
# AST definition
# ----------------------------

def sin_func(x: np.ndarray) -> np.ndarray:
    return np.sin(x)


def cos_func(x: np.ndarray) -> np.ndarray:
    return np.cos(x)


def tanh_func(x: np.ndarray) -> np.ndarray:
    return np.tanh(x)


def exp_clip_func(x: np.ndarray) -> np.ndarray:
    # Clip to prevent overflow while still allowing interesting shapes.
    return np.exp(np.clip(x, -8.0, 8.0))


def identity_func(x: np.ndarray) -> np.ndarray:
    return x


FUNCTIONS: Dict[str, Callable[[np.ndarray], np.ndarray]] = {
    "sin": sin_func,
    "cos": cos_func,
    "tanh": tanh_func,
    "exp_clip": exp_clip_func,
    "identity": identity_func,
}


@dataclass
class ThreeEdgeAST:
    function_name: str
    scalar: float
    offset: float
    arg_scale: float

    def evaluate(self, x: np.ndarray) -> np.ndarray:
        func = FUNCTIONS[self.function_name]
        return self.scalar * func(self.arg_scale * x) + self.offset

    def __str__(self) -> str:
        return (
            f"AST(function={self.function_name}, "
            f"scalar={self.scalar:.1f}, "
            f"offset={self.offset:.1f}, "
            f"arg_scale={self.arg_scale:.1f})"
        )


# ----------------------------
# Utility functions
# ----------------------------

def random_constant(step: float = 0.1, low: float = -10.0, high: float = 10.0) -> float:
    """
    Return a random constant in [low, high] with increments of 'step'.
    Example values: -10.0, -9.9, ..., 9.9, 10.0
    """
    count = int(round((high - low) / step))
    value = low + step * random.randint(0, count)
    return round(value, 1)


def generate_random_ast(function_name: str) -> ThreeEdgeAST:
    """
    Generate a random 3-edge AST. Avoid arg_scale near zero too often so the
    function isn't almost constant all the time.
    """
    scalar = random_constant()
    offset = random_constant()

    # Reject exactly 0.0 for arg_scale sometimes to make curves more interesting.
    arg_scale = random_constant()
    attempts = 0
    while abs(arg_scale) < 0.1 and attempts < 20:
        arg_scale = random_constant()
        attempts += 1

    return ThreeEdgeAST(
        function_name=function_name,
        scalar=scalar,
        offset=offset,
        arg_scale=arg_scale,
    )


def compute_stats(y_true: np.ndarray, y_pred: np.ndarray) -> dict:
    diff = y_true - y_pred
    mae = float(np.mean(np.abs(diff)))
    mse = float(np.mean(diff ** 2))
    rmse = float(math.sqrt(mse))
    return {"MAE": mae, "MSE": mse, "RMSE": rmse}


def print_stats(label: int, ast: ThreeEdgeAST, stats: dict) -> None:
    print(f"Computed AST #{label}: {ast}")
    print(f"  MAE : {stats['MAE']:.6f}")
    print(f"  MSE : {stats['MSE']:.6f}")
    print(f"  RMSE: {stats['RMSE']:.6f}")
    print("-" * 60)


# ----------------------------
# Main interactive plot logic
# ----------------------------

def main() -> None:
    random.seed()

    x = np.linspace(X_MIN, X_MAX, NUM_POINTS)

    # Fixed test AST and test data
    test_ast = generate_random_ast(FUNCTION_NAME)
    y_test = test_ast.evaluate(x)

    print("Test data AST:")
    print(test_ast)
    print("=" * 60)
    print("Press Enter in the plot window to generate another computed AST.")
    print("Close the plot window to exit.")
    print("=" * 60)

    fig, ax = plt.subplots(figsize=(11, 6))
    fig.canvas.manager.set_window_title("3-Edge AST Comparison")

    # Plot fixed test data
    ax.plot(x, y_test, color="blue", linewidth=2.5, label="test data")
    ax.set_title("3-Edge AST: Test Data vs Computed Data")
    ax.set_xlabel("x")
    ax.set_ylabel("y")
    ax.grid(True, alpha=0.3)

    computed_count = 0
    computed_lines: List = []

    def add_computed_curve() -> None:
        nonlocal computed_count

        computed_count += 1
        computed_ast = generate_random_ast(FUNCTION_NAME)
        y_computed = computed_ast.evaluate(x)
        stats = compute_stats(y_test, y_computed)

        print_stats(computed_count, computed_ast, stats)

        # Keep only the last TWO computed curves
        while len(computed_lines) > 2:
            old_line = computed_lines.pop(0)
            old_line.remove()

        # Recolor remaining curves
        if len(computed_lines) > 1:
            # Older (index 0)
            computed_lines[0].set_color("gray")
            computed_lines[0].set_alpha(0.6)

            # Newest (index 1)
            computed_lines[1].set_color("red")
            computed_lines[1].set_alpha(1.0)

        # Plot computed curve in red, with runtime-order label
        line, = ax.plot(
            x,
            y_computed,
            color="red",
            linewidth=1.6,
            alpha=0.85,
            label=str(computed_count),
        )
        computed_lines.append(line)

        # Rebuild legend cleanly
        ax.legend(loc="best")
        fig.canvas.draw_idle()

    def on_key(event) -> None:
        if event.key == "enter":
            add_computed_curve()

    # Generate and plot the first computed AST immediately
    add_computed_curve()

    fig.canvas.mpl_connect("key_press_event", on_key)
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()


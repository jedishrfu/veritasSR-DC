#!/usr/bin/env python3

import argparse
import ast
import csv
import sys
import tkinter as tk
import tkinter.font as tkfont
from tkinter import messagebox

import numpy as np
import matplotlib

matplotlib.use("TkAgg")

import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg


plt.rcParams.update({
    "font.size": 8,
    "axes.titlesize": 9,
    "axes.labelsize": 8,
    "xtick.labelsize": 7,
    "ytick.labelsize": 7,
    "legend.fontsize": 7,
})


HELP_TEXT = """
comp-expr-csv-2-fp32.py

Usage:
    python3 comp-expr-csv-2-fp32.py expr_array_stats.csv data.f32

Arguments:
    expr_array_stats.csv   Pipe-separated CSV file created by writeCsvExprArrayStats()
                           The expression must be the last field.
    data.f32               Binary file containing 32-bit floating point values

Expected CSV format:
    #|MaxAbsoluteError|#nodes|Depth|MAE|MSE|RMSE|PSNR|...|Expression

Allowed expressions:
    Variable:   x
    Functions:  exp(x), log(x), sin(x), cos(x)
    Operators:  +, -, *, /, ^

Controls:
    Mouse click expression     Select expression
    Up arrow                   Zoom out by power of 10
    Down arrow                 Zoom in by power of 10
    Right arrow                Page right
    Left arrow                 Page left
    Home                       Go to beginning
    End                        Go to end

Plot colors:
    Red      Selected expression
    Blue     Binary float32 data
"""


SAFE_FUNCS = {
    "exp": np.exp,
    "log": np.log,
    "sin": np.sin,
    "cos": np.cos,
}


class SafeExpression:
    def __init__(self, expression, stats=None, headers=None):
        self.original_expression = expression.strip()
        self.expression = self.original_expression.replace("^", "**")
        self.stats = stats if stats is not None else {}
        self.headers = headers if headers is not None else []

        tree = self.validate(self.expression)
        self.code = compile(tree, "<expression>", "eval")

    def validate(self, expr):
        tree = ast.parse(expr, mode="eval")

        allowed_nodes = (
            ast.Expression,
            ast.BinOp,
            ast.UnaryOp,
            ast.Constant,
            ast.Name,
            ast.Load,
            ast.Call,
            ast.Add,
            ast.Sub,
            ast.Mult,
            ast.Div,
            ast.Pow,
            ast.USub,
            ast.UAdd,
        )

        allowed_binary_ops = (
            ast.Add,
            ast.Sub,
            ast.Mult,
            ast.Div,
            ast.Pow,
        )

        allowed_unary_ops = (
            ast.USub,
            ast.UAdd,
        )

        for node in ast.walk(tree):
            if not isinstance(node, allowed_nodes):
                raise ValueError(f"disallowed syntax: {expr}")

            if isinstance(node, ast.BinOp):
                if not isinstance(node.op, allowed_binary_ops):
                    raise ValueError("only +, -, *, /, and ^ are allowed")

            if isinstance(node, ast.UnaryOp):
                if not isinstance(node.op, allowed_unary_ops):
                    raise ValueError("only unary + and - are allowed")

            if isinstance(node, ast.Name):
                if node.id != "x" and node.id not in SAFE_FUNCS:
                    raise ValueError(f"unknown name '{node.id}'")

            if isinstance(node, ast.Call):
                if not isinstance(node.func, ast.Name):
                    raise ValueError("only simple function calls are allowed")

                if node.func.id not in SAFE_FUNCS:
                    raise ValueError(
                        f"function '{node.func.id}' is not allowed; "
                        "allowed functions are exp, log, sin, cos"
                    )

                if len(node.args) != 1:
                    raise ValueError(
                        f"function '{node.func.id}' requires exactly one argument"
                    )

                if node.keywords:
                    raise ValueError("keyword arguments are not allowed")

        return tree

    def eval(self, x):
        env = dict(SAFE_FUNCS)
        env["x"] = x
        return eval(self.code, {"__builtins__": {}}, env)


def show_help_panel():
    root = tk.Tk()
    root.title("comp-expr-csv-2-fp32.py Help")

    try:
        root.tk.call("tk", "scaling", 1.0)
    except Exception:
        pass

    screen_w = root.winfo_screenwidth()
    screen_h = root.winfo_screenheight()
    root.geometry(f"{int(screen_w * 0.65)}x{int(screen_h * 0.65)}")

    smallfont = tkfont.Font(family="Helvetica", size=10)

    text = tk.Text(root, wrap="word", font=smallfont)
    text.pack(fill="both", expand=True, padx=10, pady=10)

    text.insert("1.0", HELP_TEXT.strip())
    text.config(state="disabled")

    button = tk.Button(root, text="Close", command=root.destroy, font=smallfont)
    button.pack(pady=(0, 10))

    root.mainloop()


def split_pipe_csv_line(line):
    return next(csv.reader([line], delimiter="|"))


def load_expressions(filename):
    expressions = []
    headers = None

    with open(filename, "r", encoding="utf-8", newline="") as f:
        for line_number, raw_line in enumerate(f, start=1):
            line = raw_line.strip()

            if not line:
                continue

            fields = [field.strip() for field in split_pipe_csv_line(line)]

            if len(fields) < 2:
                print(f"Skipping line {line_number}: not enough fields")
                continue

            if headers is None:
                headers = fields
                continue

            if len(fields) != len(headers):
                print(
                    f"Skipping line {line_number}: expected {len(headers)} fields, "
                    f"got {len(fields)}"
                )
                continue

            expression_text = fields[-1].strip()

            if not expression_text or expression_text.lower().startswith("no expressions"):
                print(f"Skipping line {line_number}: no expression")
                continue

            stats = {}
            for header, value in zip(headers[:-1], fields[:-1]):
                stats[header.strip()] = value.strip()

            try:
                expressions.append(
                    SafeExpression(
                        expression_text,
                        stats=stats,
                        headers=headers[:-1],
                    )
                )
            except Exception as e:
                print(f"Skipping line {line_number}: {e}")

    if not expressions:
        raise ValueError("No valid expressions found in CSV file.")

    return expressions


def load_float32_binary(filename):
    data = np.fromfile(filename, dtype=np.float32)

    if data.size == 0:
        raise ValueError("Binary file contains no float32 values.")

    return data


class PlotApp:
    def __init__(self, root, expressions, data):
        self.root = root
        self.expressions = expressions
        self.data = data
        self.n = len(data)

        self.selected_index = 0
        self.zoom_power = 3
        self.start = 0

        self.root.title("Expression CSV Stats vs Float32 Data Plotter")

        try:
            self.root.tk.call("tk", "scaling", 1.0)
        except Exception:
            pass

        screen_w = self.root.winfo_screenwidth()
        screen_h = self.root.winfo_screenheight()

        win_w = int(screen_w * 0.92)
        win_h = int(screen_h * 0.85)

        self.root.geometry(f"{win_w}x{win_h}")

        self.smallfont = tkfont.Font(family="Helvetica", size=10)
        self.statsfont = tkfont.Font(family="Menlo", size=10)

        self.main_frame = tk.Frame(root)
        self.main_frame.pack(fill="both", expand=True)

        self.left_frame = tk.Frame(self.main_frame, width=270)
        self.left_frame.pack(side="left", fill="y")
        self.left_frame.pack_propagate(False)

        self.stats_frame = tk.Frame(self.main_frame, width=260, relief="groove", borderwidth=1)
        self.stats_frame.pack(side="left", fill="y", padx=(0, 5))
        self.stats_frame.pack_propagate(False)

        self.right_frame = tk.Frame(self.main_frame)
        self.right_frame.pack(side="right", fill="both", expand=True)

        self.expr_label = tk.Label(
            self.left_frame,
            text="Expressions",
            anchor="w",
            font=self.smallfont,
        )
        self.expr_label.pack(fill="x", padx=5, pady=(5, 2))

        self.listbox_container = tk.Frame(self.left_frame)
        self.listbox_container.pack(fill="both", expand=True)

        self.listbox = tk.Listbox(
            self.listbox_container,
            width=32,
            exportselection=False,
            font=self.smallfont,
        )
        self.listbox.pack(side="left", fill="both", expand=True, padx=(5, 0), pady=5)

        scrollbar = tk.Scrollbar(self.listbox_container, orient="vertical")
        scrollbar.pack(side="right", fill="y", pady=5, padx=(0, 5))

        self.listbox.config(yscrollcommand=scrollbar.set)
        scrollbar.config(command=self.listbox.yview)

        for i, expr in enumerate(self.expressions):
            expr_num = expr.stats.get("#", str(i))
            maxae = expr.stats.get("MaxAbsoluteError", "")
            nodes = expr.stats.get("#nodes", "")

            label = f"{expr_num}: AE={maxae} N={nodes}  {expr.original_expression}"
            self.listbox.insert(tk.END, label)

        self.listbox.selection_set(0)
        self.listbox.bind("<<ListboxSelect>>", self.on_select)

        self.stats_label = tk.Label(
            self.stats_frame,
            text="Selected Stats",
            anchor="w",
            font=self.smallfont,
        )
        self.stats_label.pack(fill="x", padx=5, pady=(5, 2))

        self.stats_text = tk.Text(
            self.stats_frame,
            wrap="word",
            font=self.statsfont,
            height=20,
            width=30,
        )
        self.stats_text.pack(fill="both", expand=True, padx=5, pady=5)
        self.stats_text.config(state="disabled")

        self.fig, self.ax = plt.subplots(
            figsize=(8, 5),
            dpi=100,
            constrained_layout=True,
        )

        self.red_line, = self.ax.plot([], [], "r-", linewidth=0.9, label="expression")
        self.blue_line, = self.ax.plot([], [], "b-", linewidth=0.9, label="float32 data")

        self.ax.legend(loc="best")

        self.canvas = FigureCanvasTkAgg(self.fig, master=self.right_frame)
        self.canvas.get_tk_widget().pack(fill="both", expand=True)

        self.status = tk.Label(root, anchor="w", font=self.smallfont)
        self.status.pack(fill="x")

        self.root.bind("<Up>", self.on_key)
        self.root.bind("<Down>", self.on_key)
        self.root.bind("<Left>", self.on_key)
        self.root.bind("<Right>", self.on_key)
        self.root.bind("<Home>", self.on_key)
        self.root.bind("<End>", self.on_key)

        self.update_stats_panel()
        self.update_plot()

    def window_size(self):
        return min(10 ** self.zoom_power, self.n)

    def clamp_start(self):
        w = self.window_size()
        self.start = max(0, min(self.start, max(0, self.n - w)))

    def update_stats_panel(self):
        expr = self.expressions[self.selected_index]

        self.stats_text.config(state="normal")
        self.stats_text.delete("1.0", tk.END)

        self.stats_text.insert(tk.END, "Expression #\n")
        self.stats_text.insert(tk.END, "------------\n")
        self.stats_text.insert(tk.END, f"{expr.stats.get('#', self.selected_index)}\n\n")

        self.stats_text.insert(tk.END, "Metrics\n")
        self.stats_text.insert(tk.END, "-------\n")

        for key in expr.headers:
            value = expr.stats.get(key, "")
            self.stats_text.insert(tk.END, f"{key}: {value}\n")

        self.stats_text.insert(tk.END, "\nExpression\n")
        self.stats_text.insert(tk.END, "----------\n")
        self.stats_text.insert(tk.END, expr.original_expression)

        self.stats_text.config(state="disabled")

    def update_plot(self):
        self.clamp_start()

        w = self.window_size()
        end = min(self.start + w, self.n)

        x = np.arange(self.start, end, dtype=np.float64)
        y_data = self.data[self.start:end]

        expr = self.expressions[self.selected_index]

        try:
            with np.errstate(all="ignore"):
                y_expr = expr.eval(x)

            if np.isscalar(y_expr):
                y_expr = np.full_like(x, y_expr, dtype=np.float64)

            y_expr = np.asarray(y_expr, dtype=np.float64)

            if y_expr.shape != x.shape:
                raise ValueError(
                    f"expression returned shape {y_expr.shape}, expected {x.shape}"
                )

        except Exception as e:
            messagebox.showerror("Expression Error", str(e))
            return

        self.red_line.set_data(x, y_expr)
        self.blue_line.set_data(x, y_data)

        self.ax.relim()
        self.ax.autoscale_view()

        expr_num = expr.stats.get("#", self.selected_index)
        title = f"Expr {expr_num}: {expr.original_expression}"
        if len(title) > 90:
            title = title[:87] + "..."

        self.ax.set_title(title)
        self.ax.set_xlabel("index")
        self.ax.set_ylabel("value")

        self.status.config(
            text=(
                f"indices {self.start:,} to {end - 1:,}    "
                f"window = 10^{self.zoom_power} = {w:,} points    "
                f"total = {self.n:,}"
            )
        )

        self.canvas.draw_idle()

    def on_select(self, event):
        selection = self.listbox.curselection()
        if selection:
            self.selected_index = selection[0]
            self.update_stats_panel()
            self.update_plot()

    def on_key(self, event):
        w = self.window_size()

        if event.keysym == "Up":
            self.zoom_power = min(10, self.zoom_power + 1)

        elif event.keysym == "Down":
            self.zoom_power = max(1, self.zoom_power - 1)

        elif event.keysym == "Right":
            self.start += w

        elif event.keysym == "Left":
            self.start -= w

        elif event.keysym == "Home":
            self.start = 0

        elif event.keysym == "End":
            self.start = max(0, self.n - w)

        self.update_plot()


def parse_args():
    if len(sys.argv) == 1 or sys.argv[1] in ("-h", "--help"):
        show_help_panel()
        sys.exit(0)

    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("expressions_csv_file")
    parser.add_argument("float32_file")
    return parser.parse_args()


def main():
    args = parse_args()

    expressions = load_expressions(args.expressions_csv_file)
    data = load_float32_binary(args.float32_file)

    root = tk.Tk()
    PlotApp(root, expressions, data)
    root.mainloop()


if __name__ == "__main__":
    main()

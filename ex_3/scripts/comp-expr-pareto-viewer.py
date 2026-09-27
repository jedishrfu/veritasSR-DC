#!/usr/bin/env python3

import argparse
import ast
import csv
import math
import sys
import tkinter as tk
import tkinter.font as tkfont
from tkinter import messagebox
from tkinter import ttk

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
comp-expr-pareto-viewer.py

Usage:
    python3 comp-expr-pareto-viewer.py expr_array_stats.csv data.f32
    python3 comp-expr-pareto-viewer.py -x "#nodes" -y MaxAbsoluteError expr_array_stats.csv data.f32

Arguments:
    expr_array_stats.csv   Pipe-separated CSV file created by writeCsvExprArrayStats()
                           The expression must be the last field.
    data.f32               Binary file containing 32-bit floating point values

Expected CSV format:
    #|MaxAbsoluteError|#nodes|Depth|MAE|MSE|RMSE|PSNR|...|Expression

Default Pareto chart:
    x-axis: #nodes
    y-axis: MaxAbsoluteError

Options:
    -x FIELD     CSV statistic to use as Pareto x-axis. Default: #nodes
    -y FIELD     CSV statistic to use as Pareto y-axis. Default: MaxAbsoluteError

Allowed expressions:
    Variable:   x
    Functions:  exp(x), log(x), sin(x), cos(x)
    Operators:  +, -, *, /, ^

Controls:
    Top-left table click       Select expression
    Pareto point click         Select expression
    Data plot click            Give keyboard focus to data plot controls
    Up arrow                   Zoom out by power of 10
    Down arrow                 Zoom in by power of 10
    Right arrow                Page right
    Left arrow                 Page left
    Home                       Go to beginning
    End                        Go to end
"""


SAFE_FUNCS = {
    "exp": np.exp,
    "log": np.log,
    "sin": np.sin,
    "cos": np.cos,
}


def normalize_header_name(name):
    return name.strip().lower().replace(" ", "")


def find_header(headers, requested_name):
    requested = normalize_header_name(requested_name)

    for header in headers:
        if normalize_header_name(header) == requested:
            return header

    aliases = {
        "maxabserr": "MaxAbsoluteError",
        "maxabsoluteerror": "MaxAbsoluteError",
        "nodes": "#nodes",
        "#nodes": "#nodes",
        "nodecount": "#nodes",
        "#": "#",
        "expr#": "#",
        "expression#": "#",
    }

    if requested in aliases:
        target = normalize_header_name(aliases[requested])
        for header in headers:
            if normalize_header_name(header) == target:
                return header

    return None


def parse_float(value):
    try:
        return float(str(value).strip())
    except Exception:
        return math.nan


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
    root.title("comp-expr-pareto-viewer.py Help")

    try:
        root.tk.call("tk", "scaling", 1.0)
    except Exception:
        pass

    screen_w = root.winfo_screenwidth()
    screen_h = root.winfo_screenheight()
    root.geometry(f"{int(screen_w * 0.72)}x{int(screen_h * 0.72)}")

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

    if headers is None:
        raise ValueError("CSV file appears to be empty.")

    if not expressions:
        raise ValueError("No valid expressions found in CSV file.")

    return expressions, headers[:-1]


def load_float32_binary(filename):
    data = np.fromfile(filename, dtype=np.float32)

    if data.size == 0:
        raise ValueError("Binary file contains no float32 values.")

    return data


class PlotApp:
    def __init__(self, root, expressions, headers, data, pareto_x_name, pareto_y_name):
        self.root = root
        self.expressions = expressions
        self.headers = headers
        self.data = data
        self.n = len(data)

        self.pareto_x_name = pareto_x_name
        self.pareto_y_name = pareto_y_name

        self.selected_index = 0
        self.zoom_power = 3
        self.start = 0

        self.root.title("Expression CSV Pareto Viewer")

        try:
            self.root.tk.call("tk", "scaling", 1.0)
        except Exception:
            pass

        screen_w = self.root.winfo_screenwidth()
        screen_h = self.root.winfo_screenheight()

        win_w = int(screen_w * 0.94)
        win_h = int(screen_h * 0.88)

        self.root.geometry(f"{win_w}x{win_h}")
        self.root.minsize(900, 650)
        self.root.resizable(True, True)

        self.smallfont = tkfont.Font(family="Helvetica", size=10)
        self.statsfont = tkfont.Font(family="Menlo", size=10)

        self.main = tk.PanedWindow(root, orient=tk.HORIZONTAL, sashrelief=tk.RAISED)
        self.main.pack(fill="both", expand=True)

        self.left_pane = tk.PanedWindow(self.main, orient=tk.VERTICAL, sashrelief=tk.RAISED)
        self.right_pane = tk.PanedWindow(self.main, orient=tk.VERTICAL, sashrelief=tk.RAISED)

        self.main.add(self.left_pane, minsize=420)
        self.main.add(self.right_pane, minsize=500)

        self.table_frame = tk.Frame(self.left_pane)
        self.stats_frame = tk.Frame(self.left_pane)
        self.data_plot_frame = tk.Frame(self.right_pane)
        self.pareto_plot_frame = tk.Frame(self.right_pane)

        self.left_pane.add(self.table_frame, minsize=260)
        self.left_pane.add(self.stats_frame, minsize=220)
        self.right_pane.add(self.data_plot_frame, minsize=300)
        self.right_pane.add(self.pareto_plot_frame, minsize=260)

        self.build_table_pane()
        self.build_stats_pane()
        self.build_data_plot_pane()
        self.build_pareto_plot_pane()

        self.status = tk.Label(root, anchor="w", font=self.smallfont)
        self.status.pack(fill="x")

        self.root.bind("<Up>", self.on_key)
        self.root.bind("<Down>", self.on_key)
        self.root.bind("<Left>", self.on_key)
        self.root.bind("<Right>", self.on_key)
        self.root.bind("<Home>", self.on_key)
        self.root.bind("<End>", self.on_key)
        self.root.bind("<Configure>", self.on_window_configure)

        self._resize_after_id = None

        self.root.after(100, self.set_initial_sash_positions)
        self.update_all()


    def set_initial_sash_positions(self):
        try:
            width = max(self.root.winfo_width(), 900)
            height = max(self.root.winfo_height(), 650)

            self.main.sash_place(0, int(width * 0.43), 0)
            self.left_pane.sash_place(0, 0, int(height * 0.50))
            self.right_pane.sash_place(0, 0, int(height * 0.50))
        except Exception:
            pass


    def on_window_configure(self, event):
        if event.widget is not self.root:
            return

        if self._resize_after_id is not None:
            try:
                self.root.after_cancel(self._resize_after_id)
            except Exception:
                pass

        self._resize_after_id = self.root.after(150, self.redraw_after_resize)

    def redraw_after_resize(self):
        self._resize_after_id = None

        try:
            self.data_canvas.draw_idle()
            self.pareto_canvas.draw_idle()
        except Exception:
            pass

    def build_table_pane(self):
        # This pane uses ONLY pack() inside table_frame.
        title = tk.Label(
            self.table_frame,
            text="Expressions",
            anchor="w",
            font=self.smallfont,
        )
        title.pack(fill="x", padx=5, pady=(5, 2))

        table_area = tk.Frame(self.table_frame)
        table_area.pack(fill="both", expand=True, padx=5, pady=5)

        columns = ("expr_num", "max_abs_error", "nodes", "expression")

        self.tree = ttk.Treeview(
            table_area,
            columns=columns,
            show="headings",
            selectmode="browse",
        )

        self.tree.heading("expr_num", text="#")
        self.tree.heading("max_abs_error", text="MaxAbsError")
        self.tree.heading("nodes", text="#nodes")
        self.tree.heading("expression", text="Expression")

        self.tree.column("expr_num", width=55, stretch=False, anchor="e")
        self.tree.column("max_abs_error", width=105, stretch=False, anchor="e")
        self.tree.column("nodes", width=70, stretch=False, anchor="e")
        self.tree.column("expression", width=360, stretch=True, anchor="w")

        vsb = ttk.Scrollbar(table_area, orient="vertical", command=self.tree.yview)
        hsb = ttk.Scrollbar(self.table_frame, orient="horizontal", command=self.tree.xview)

        self.tree.configure(yscrollcommand=vsb.set, xscrollcommand=hsb.set)

        self.tree.pack(side="left", fill="both", expand=True)
        vsb.pack(side="right", fill="y")
        hsb.pack(side="bottom", fill="x", padx=5, pady=(0, 5))

        for i, expr in enumerate(self.expressions):
            expr_num = expr.stats.get("#", str(i))
            maxae = expr.stats.get("MaxAbsoluteError", expr.stats.get("MaxAbsError", ""))
            nodes = expr.stats.get("#nodes", expr.stats.get("Nodes", ""))

            self.tree.insert(
                "",
                "end",
                iid=str(i),
                values=(expr_num, maxae, nodes, expr.original_expression),
            )

        self.tree.bind("<<TreeviewSelect>>", self.on_table_select)

        if self.expressions:
            self.tree.selection_set("0")
            self.tree.focus("0")

    def build_stats_pane(self):
        # This pane uses ONLY pack() inside stats_frame.
        title = tk.Label(
            self.stats_frame,
            text="Selected Expression Stats",
            anchor="w",
            font=self.smallfont,
        )
        title.pack(fill="x", padx=5, pady=(5, 2))

        body = tk.Frame(self.stats_frame)
        body.pack(fill="both", expand=True, padx=5, pady=5)

        self.stats_text = tk.Text(
            body,
            wrap="word",
            font=self.statsfont,
            height=20,
            width=40,
        )

        scrollbar = tk.Scrollbar(body, orient="vertical", command=self.stats_text.yview)

        self.stats_text.config(yscrollcommand=scrollbar.set)

        self.stats_text.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        self.stats_text.config(state="disabled")

    def build_data_plot_pane(self):
        self.data_fig, self.data_ax = plt.subplots(
            figsize=(7, 3.5),
            dpi=100,
        )
        self.data_fig.subplots_adjust(
            left=0.10,
            right=0.98,
            bottom=0.16,
            top=0.86,
        )

        self.expr_line, = self.data_ax.plot(
            [],
            [],
            "r-",
            linewidth=0.9,
            label="expression",
        )

        self.data_line, = self.data_ax.plot(
            [],
            [],
            "b-",
            linewidth=0.9,
            label="float32 data",
        )

        self.data_ax.legend(loc="best")

        self.data_canvas = FigureCanvasTkAgg(self.data_fig, master=self.data_plot_frame)
        data_widget = self.data_canvas.get_tk_widget()
        data_widget.pack(fill="both", expand=True)
        data_widget.configure(takefocus=True)

        self.data_canvas.mpl_connect("button_press_event", self.on_data_plot_click)

    def build_pareto_plot_pane(self):
        self.pareto_fig, self.pareto_ax = plt.subplots(
            figsize=(7, 3.5),
            dpi=100,
        )
        self.pareto_fig.subplots_adjust(
            left=0.10,
            right=0.98,
            bottom=0.18,
            top=0.86,
        )

        self.pareto_line, = self.pareto_ax.plot(
            [],
            [],
            "b-o",
            linewidth=2.5,
            markersize=3.5,
            label="expressions",
        )

        self.selected_pareto_point, = self.pareto_ax.plot(
            [],
            [],
            "ro",
            markersize=5,
            label="selected",
        )

        self.pareto_ax.legend(loc="best")

        self.pareto_canvas = FigureCanvasTkAgg(self.pareto_fig, master=self.pareto_plot_frame)
        pareto_widget = self.pareto_canvas.get_tk_widget()
        pareto_widget.pack(fill="both", expand=True)
        pareto_widget.configure(takefocus=True)

        self.pareto_canvas.mpl_connect("button_press_event", self.on_pareto_click)

        self.pareto_x_values = np.array(
            [parse_float(expr.stats.get(self.pareto_x_name)) for expr in self.expressions],
            dtype=np.float64,
        )

        self.pareto_y_values = np.array(
            [parse_float(expr.stats.get(self.pareto_y_name)) for expr in self.expressions],
            dtype=np.float64,
        )

    def window_size(self):
        return min(10 ** self.zoom_power, self.n)

    def clamp_start(self):
        w = self.window_size()
        self.start = max(0, min(self.start, max(0, self.n - w)))

    def update_all(self):
        self.update_table_selection()
        self.update_stats_panel()
        self.update_data_plot()
        self.update_pareto_plot()

    def update_table_selection(self):
        iid = str(self.selected_index)

        if self.tree.exists(iid):
            current = self.tree.selection()
            if current != (iid,):
                self.tree.selection_set(iid)
            self.tree.focus(iid)
            self.tree.see(iid)

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

    def update_data_plot(self):
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

        self.expr_line.set_data(x, y_expr)
        self.data_line.set_data(x, y_data)

        self.data_ax.relim()
        self.data_ax.autoscale_view()

        expr_num = expr.stats.get("#", self.selected_index)
        title = f"Data vs Expression #{expr_num}: {expr.original_expression}"
        if len(title) > 100:
            title = title[:97] + "..."

        self.data_ax.set_title(title)
        self.data_ax.set_xlabel("index")
        self.data_ax.set_ylabel("value")

        self.status.config(
            text=(
                f"indices {self.start:,} to {end - 1:,}    "
                f"window = 10^{self.zoom_power} = {w:,} points    "
                f"total = {self.n:,}    "
                f"Pareto: x={self.pareto_x_name}, y={self.pareto_y_name}"
            )
        )

        self.data_canvas.draw_idle()

    def update_pareto_plot(self):
        valid = np.isfinite(self.pareto_x_values) & np.isfinite(self.pareto_y_values)

        xs = self.pareto_x_values[valid]
        ys = self.pareto_y_values[valid]

        self.pareto_line.set_data(xs, ys)

        sx = self.pareto_x_values[self.selected_index]
        sy = self.pareto_y_values[self.selected_index]

        if np.isfinite(sx) and np.isfinite(sy):
            self.selected_pareto_point.set_data([sx], [sy])
        else:
            self.selected_pareto_point.set_data([], [])

        self.pareto_ax.relim()
        self.pareto_ax.autoscale_view()

        self.pareto_ax.set_title("Pareto Front")
        self.pareto_ax.set_xlabel(self.pareto_x_name)
        self.pareto_ax.set_ylabel(self.pareto_y_name)
        self.pareto_ax.grid(True)

        self.pareto_canvas.draw_idle()

    def select_expression(self, index):
        if index < 0 or index >= len(self.expressions):
            return

        self.selected_index = index
        self.update_all()

    def on_table_select(self, event):
        selection = self.tree.selection()

        if not selection:
            return

        try:
            index = int(selection[0])
        except ValueError:
            return

        if index != self.selected_index:
            self.select_expression(index)

    def on_data_plot_click(self, event):
        self.data_canvas.get_tk_widget().focus_set()

    def on_pareto_click(self, event):
        if event.inaxes != self.pareto_ax:
            return

        if event.xdata is None or event.ydata is None:
            return

        finite_indices = np.where(
            np.isfinite(self.pareto_x_values) & np.isfinite(self.pareto_y_values)
        )[0]

        if finite_indices.size == 0:
            return

        points = np.column_stack((
            self.pareto_x_values[finite_indices],
            self.pareto_y_values[finite_indices],
        ))

        clicked = np.array([event.xdata, event.ydata], dtype=np.float64)

        xlim = self.pareto_ax.get_xlim()
        ylim = self.pareto_ax.get_ylim()

        xspan = max(abs(xlim[1] - xlim[0]), 1e-12)
        yspan = max(abs(ylim[1] - ylim[0]), 1e-12)

        normalized_points = np.column_stack((
            (points[:, 0] - clicked[0]) / xspan,
            (points[:, 1] - clicked[1]) / yspan,
        ))

        distances = np.sqrt(np.sum(normalized_points * normalized_points, axis=1))
        nearest_pos = int(np.argmin(distances))
        nearest_index = int(finite_indices[nearest_pos])

        self.select_expression(nearest_index)

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

        else:
            return

        self.update_data_plot()


def parse_args():
    if len(sys.argv) == 1 or sys.argv[1] in ("-h", "--help"):
        show_help_panel()
        sys.exit(0)

    parser = argparse.ArgumentParser(
        description="View expression CSV stats, data comparison, and Pareto chart.",
    )

    parser.add_argument(
        "-x",
        "--xcoef",
        default="#nodes",
        help='CSV stat to use as Pareto x-axis. Default: "#nodes"',
    )

    parser.add_argument(
        "-y",
        "--ycoef",
        default="MaxAbsoluteError",
        help='CSV stat to use as Pareto y-axis. Default: "MaxAbsoluteError"',
    )

    parser.add_argument("expressions_csv_file")
    parser.add_argument("float32_file")

    return parser.parse_args()


def main():
    args = parse_args()

    expressions, headers = load_expressions(args.expressions_csv_file)
    data = load_float32_binary(args.float32_file)

    pareto_x_name = find_header(headers, args.xcoef)
    pareto_y_name = find_header(headers, args.ycoef)

    if pareto_x_name is None:
        print("Available CSV stats:")
        for h in headers:
            print(f"  {h}")
        raise ValueError(f"Could not find x-axis coefficient/stat '{args.xcoef}'")

    if pareto_y_name is None:
        print("Available CSV stats:")
        for h in headers:
            print(f"  {h}")
        raise ValueError(f"Could not find y-axis coefficient/stat '{args.ycoef}'")

    root = tk.Tk()
    PlotApp(root, expressions, headers, data, pareto_x_name, pareto_y_name)
    root.mainloop()


if __name__ == "__main__":
    main()

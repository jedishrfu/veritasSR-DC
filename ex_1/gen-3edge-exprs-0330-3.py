#!/usr/bin/env python3

from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable, List, Optional, Set
import argparse


INTRINSICS = [
    "null",
    "identity",
    "sin",
    "cos",
    "tan",
    "exp",
    "log",
    "sqrt",
    "abs",
    "asin",
    "acos",
    "atan",
    "sinh",
    "cosh",
    "tanh",
]


def is_valid_intrinsic(name: str) -> bool:
    return name in INTRINSICS


@dataclass(frozen=True)
class ExprNode:
    level: int
    intrinsic: str
    power: int
    offset: Optional["ExprNode"] = None

    def is_base_constant(self) -> bool:
        return self.offset is None

    def to_string(self) -> str:
        if self.offset is None:
            return "b0"
        term = self._term_string()
        return f"{term} + {self.offset.to_string()}"

    def _term_string(self) -> str:
        a = f"a{self.level}"

        if self.intrinsic == "null":
            return ""

        x_term = "x" if self.power == 1 else f"x ^ {self.power}"

        if self.intrinsic == "identity":
            return f"{a} * {x_term}"

        return f"{a} * {self.intrinsic}( {x_term} )"

    def pretty_tree(self, indent: int = 0) -> str:
        pad = "  " * indent
        lines = [
            f"{pad}ExprNode( level = {self.level}, power = {self.power} )",
            f"{pad}  left-edge   scale     = a{self.level}" if self.level >= 0 else f"{pad}  left-edge   scale     = none",
            f"{pad}  middle      intrinsic = {self.intrinsic}",
            f"{pad}  right-edge  offset    = {'subtree' if self.offset is not None else 'b0'}",
        ]
        if self.offset is not None:
            lines.append(f"{pad}  offset subtree:")
            lines.append(self.offset.pretty_tree(indent + 2))
        return "\n".join(lines)


def base_leaf() -> ExprNode:
    return ExprNode(level=-1, intrinsic="null", power=0, offset=None)


def dedupe_nodes(nodes: Iterable[ExprNode]) -> List[ExprNode]:
    seen: Set[str] = set()
    result: List[ExprNode] = []
    for node in nodes:
        s = node.to_string()
        if s not in seen:
            seen.add(s)
            result.append(node)
    return result


def generate_level_zero(intrinsics: Iterable[str]) -> List[ExprNode]:
    """
    Level 0 amino acids:
      b0
      a0 * x + b0
      a0 * sin( x ) + b0
      ...
    """
    leaf = base_leaf()
    nodes: List[ExprNode] = [leaf]

    for intrinsic in intrinsics:
        if intrinsic == "null":
            continue
        nodes.append(
            ExprNode(
                level=0,
                intrinsic=intrinsic,
                power=1,   # leaf expression nodes keep power 1
                offset=leaf,
            )
        )

    return dedupe_nodes(nodes)


def generate_level(level: int, intrinsics: Iterable[str]) -> List[ExprNode]:
    if level < 0:
        raise ValueError("level must be >= 0")

    if level == 0:
        return generate_level_zero(intrinsics)

    prev = generate_level(level - 1, intrinsics)
    nodes: List[ExprNode] = []

    for child in prev:
        for intrinsic in intrinsics:
            if intrinsic == "null":
                continue

            # If child is just b0, this new node is a leaf expression node.
            # Keep power at 1.
            if child.is_base_constant():
                power = 1
            else:
                power = child.power + 1

            nodes.append(
                ExprNode(
                    level=level,
                    intrinsic=intrinsic,
                    power=power,
                    offset=child,
                )
            )

    return dedupe_nodes(nodes)


def generate_up_to_level(level: int, intrinsics: Iterable[str]) -> List[List[ExprNode]]:
    return [generate_level(k, intrinsics) for k in range(level + 1)]


def parse_intrinsics(text: str) -> List[str]:
    names = [item.strip() for item in text.split(",") if item.strip()]
    bad = [name for name in names if not is_valid_intrinsic(name)]
    if bad:
        raise ValueError(
            f"Unknown intrinsic(s): {bad}\n"
            f"Valid intrinsics are: {', '.join(INTRINSICS)}"
        )
    return names


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate 3-edge AST expressions with level-based a_i coefficients and leaf power fixed at 1."
    )
    parser.add_argument(
        "--level",
        type=int,
        required=True,
        help="Expression level to generate.",
    )
    parser.add_argument(
        "--intrinsics",
        type=str,
        default="null,identity,sin,cos,tan,exp,log,sqrt,abs,asin,acos,atan,sinh,cosh,tanh",
        help=f"Comma-separated intrinsic list. Available: {', '.join(INTRINSICS)}",
    )
    parser.add_argument(
        "--all-levels",
        action="store_true",
        help="Print levels 0 through N.",
    )
    parser.add_argument(
        "--show-tree",
        action="store_true",
        help="Print AST tree structure.",
    )
    args = parser.parse_args()

    if args.level < 0:
        raise SystemExit("Error: --level must be >= 0")

    try:
        chosen_intrinsics = parse_intrinsics(args.intrinsics)
    except ValueError as exc:
        raise SystemExit(str(exc))

    if args.all_levels:
        for level in range(args.level + 1):
            nodes = generate_level(level, chosen_intrinsics)
            print(f"\n=== LEVEL {level} ===")
            print(f"count = {len(nodes)}")
            for i, node in enumerate(nodes, start=1):
                print(f"{i:4d}. {node.to_string()}")
                if args.show_tree:
                    print(node.pretty_tree())
                    print()
    else:
        nodes = generate_level(args.level, chosen_intrinsics)
        print(f"=== LEVEL {args.level} ===")
        print(f"count = {len(nodes)}")
        for i, node in enumerate(nodes, start=1):
            print(f"{i:4d}. {node.to_string()}")
            if args.show_tree:
                print(node.pretty_tree())
                print()


if __name__ == "__main__":
    main()


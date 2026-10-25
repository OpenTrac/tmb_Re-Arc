"""
Implementation of the "ctx" command.

The code for stripping function definitions was originally developed by
Simon Lindholm for `decomp-permuter`, licensed under MIT.
"""

from __future__ import annotations

import argparse
import logging
import re
import subprocess
from pathlib import Path

from build_system.env import Environment

LOG = logging.getLogger(__name__)

_DEFAULT_OUTPUT = "ctx.c"

# Matches the start of a function definition up to its opening `{`. Parameter lists may span
# several lines. Lines starting with `#` are macro definitions, which may contain braces.
_FUNCTION_START = re.compile(r"^[^#\n]*?\s\**(\w+)\([^;{}#]*\)\s*\{", re.MULTILINE)


def add_subparser(subparsers):
    """
    Add the argument parser for the command to the action object for
    the main parser.
    """
    parser: argparse.ArgumentParser = subparsers.add_parser(
        "ctx",
        help="Generates a context file for a C file, for use with m2c and decomp.me.",
        description=(
            "Generates a context file for a C file, for use with m2c and decomp.me. "
            "The context contains every type, declaration, and macro visible to the file."
        ),
    )
    parser.add_argument(
        "c_file",
        type=Path,
        help="C file to generate the context for.",
    )
    parser.add_argument(
        "--keep-funcs",
        nargs="+",
        default=[],
        help=(
            "Functions whose definitions should be kept in the context. "
            "All other function definitions are reduced to declarations."
        ),
    )
    parser.add_argument(
        "--output",
        "-o",
        type=Path,
        default=None,
        help=f"Path to write the context to. Defaults to `{_DEFAULT_OUTPUT}` in the repository root.",
    )
    parser.set_defaults(func=_ctx_cli)


def _ctx_cli(env: Environment, args):
    """
    Handle the "ctx" command of the CLI.
    """
    output: Path = args.output or env.directories.root / _DEFAULT_OUTPUT
    output.write_text(generate_ctx(env, args.c_file, args.keep_funcs))
    LOG.info("Wrote context for `%s` to `%s`.", args.c_file, output)


def generate_ctx(env: Environment, c_file: Path, keep_funcs: list[str]) -> str:
    """
    Generate the context for the given C file.

    The definitions of all functions except `keep_funcs` are reduced to declarations.
    """
    if not c_file.is_file():
        raise FileNotFoundError(f"Missing C file: `{c_file}`")

    args = env.generate_ctx_preprocessor_args()
    source = _preprocess(env, [*args, "-dD", str(c_file.resolve())])
    predefined = _preprocess(env, [*args, "-dM", "-"])

    predefined_macros = {
        name for line in predefined.splitlines() if (name := _macro_name(line))
    }
    # `INCLUDE_ASM(...);` expands to an empty `;` line, which only adds noise.
    lines = [
        line
        for line in source.splitlines()
        if _macro_name(line) not in predefined_macros and line.strip() != ";"
    ]

    return _strip_function_definitions("\n".join(lines) + "\n", set(keep_funcs))


def _preprocess(env: Environment, args: list[str]) -> str:
    """
    Run the C preprocessor and return its output. Its standard input is empty.
    """
    result = subprocess.run(
        args,
        cwd=env.directories.root,
        input="",
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(f"C preprocessor failed:\n{result.stderr}")
    return result.stdout


def _macro_name(line: str) -> str | None:
    """
    Get the name of the macro defined by a `#define` line, if it is one.
    """
    match = re.match(r"#define\s+(\w+)", line)
    return match.group(1) if match else None


def _strip_function_definitions(source: str, keep_funcs: set[str]) -> str:
    """
    Replace the body of every function definition not in `keep_funcs` with `;`.
    """
    result = ""
    remain = source
    while (function := _FUNCTION_START.search(remain)) is not None:
        body_start = function.end() - 1
        body_end = _find_body_end(remain, body_start)

        if function.group(1) in keep_funcs:
            result += remain[: body_end + 1]
        else:
            result += remain[:body_start].rstrip() + ";"
        remain = remain[body_end + 1 :]

    return result + remain


def _find_body_end(source: str, start: int) -> int:
    """
    Given the opening `{` of a function body, find the index of its closing `}`.
    """
    level = 0
    i = start
    while i < len(source):
        char = source[i]
        if char in "\"'":
            i = _skip_literal(source, i)
            continue

        if char == "{":
            level += 1
        elif char == "}":
            level -= 1
            if level == 0:
                return i
        i += 1

    raise ValueError(f"Unbalanced braces in function body at offset {start}")


def _skip_literal(source: str, start: int) -> int:
    """
    Given the opening quote of a string or character literal, find the index after its end.
    """
    quote = source[start]
    i = start + 1
    while i < len(source) and source[i] != quote:
        i += 2 if source[i] == "\\" else 1
    return i + 1

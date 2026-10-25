"""
Implementation of the "check" command.
"""

import argparse
import logging
import hashlib
import typing
import sys
from pathlib import Path

import colorama

from build_system.env import Environment

LOG = logging.getLogger(__name__)


def add_subparser(subparsers):
    """
    Add the argument parser for the command to the action object for
    the main parser.
    """
    parser: argparse.ArgumentParser = subparsers.add_parser(
        "check",
        help="Checks whether the given binary matches the original.",
        description="Checks whether the given binary matches the original.",
    )
    parser.add_argument(
        "--binary",
        "-b",
        help="Binary to compare against original.",
        type=Path,
        required=True,
    )
    parser.add_argument(
        "--output",
        "-o",
        help="Path to output the new binary's SHA1SUM to on success.",
        type=Path,
        required=False,
        default=None,
    )
    parser.set_defaults(func=_check_cli)


def _check_cli(env: Environment, args):
    """
    Handle the "check" command of the CLI.
    """
    matching: bool = check(env, args.binary)

    if matching:
        # Use `print` here instead of logging since this command will be used via Ninja.
        print(f"{args.binary}: {colorama.Fore.GREEN}OK{colorama.Fore.RESET}")
        if args.output is not None:
            typing.cast(Path, args.output).write_text(sha1sum(args.binary))
    else:
        print(f"{args.binary}: {colorama.Fore.RED}NONMATCHING{colorama.Fore.RESET}")
        sys.exit(1)


def check(env: Environment, binary: Path) -> bool:
    """
    Determine whether the given binary matches the original binary in the environment.
    """
    original_sha1 = sha1sum(env.files.disk_rom)
    new_sha1 = sha1sum(binary)

    return original_sha1 == new_sha1


def sha1sum(path: Path) -> str:
    """
    Calculate the SHA1 hash of a file.
    """
    with path.open("rb") as f:
        return hashlib.sha1(f.read()).hexdigest()

"""
Main CLI function/module of the build system.
"""

import argparse
import logging
import logging.config
import sys
from pathlib import Path

from .operations import (
    clean,
    distclean,
    generate,
    split,
    check,
    ctx,
    patchelf,
)
from .env import Environment, Version
from .log import get_logger_config

LOG = logging.getLogger(__name__)

cli_parser = argparse.ArgumentParser(
    description="Build script / wrapper for TMB decompilation."
)
cli_parser.add_argument(
    "--version",
    "-v",
    type=Version,
    choices=[Version.US],
    default=Version.US,
    help="Version of the binary to use.",
)
cli_parser.add_argument(
    "--log-level",
    type=str,
    choices=["DEBUG", "INFO", "WARN", "ERROR", "CRITICAL"],
    default="INFO",
    help="Logging verbosity for the scripts.",
)

subparsers = cli_parser.add_subparsers(dest="subcommand_name", required=True)
clean.add_subparser(subparsers)
distclean.add_subparser(subparsers)
split.add_subparser(subparsers)
generate.add_subparser(subparsers)
check.add_subparser(subparsers)
ctx.add_subparser(subparsers)
patchelf.add_subparser(subparsers)


def cli(root: Path):
    """
    Run the CLI using the given root directory of the build.
    """
    args = cli_parser.parse_args()
    logging.config.dictConfig(get_logger_config(args.log_level))
    env = Environment.for_version(args.version, root)

    try:
        args.func(env, args)
    except Exception as e:
        LOG.critical("Exception occurred!", exc_info=True)
        LOG.critical("Exiting due to error: %s", e)
        sys.exit(1)

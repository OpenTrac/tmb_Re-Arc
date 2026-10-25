"""
Implementation of the "split" command.
"""

import argparse
import logging
import subprocess

import splat.scripts.split as splat_split

from build_system.env import Environment
from .check import sha1sum
from .distclean import distclean

LOG = logging.getLogger(__name__)


def add_subparser(subparsers):
    """
    Add the argument parser for the command to the action object for
    the main parser.
    """
    parser: argparse.ArgumentParser = subparsers.add_parser(
        "split",
        help="Invokes `splat` to split and disassemble the target binary.",
        description="Invokes `splat` to split and disassemble the target binary.",
    )
    parser.add_argument(
        "--clean",
        "-c",
        help="Distclean the environment before splitting.",
        action="store_true",
    )
    parser.set_defaults(func=_split_cli)


def _split_cli(env: Environment, args):
    """
    Handle the "split" command of the CLI.
    """
    split(env, clean_first=args.clean)
    LOG.info("Done splitting binary.")


def _prepare_rom(env: Environment):
    """
    Create the "ROM" file for the original executable.
    """
    if not env.files.disk.is_file():
        raise FileNotFoundError(
            f"Missing required original binary file: `{env.files.disk}`"
        )

    disk_sha1 = sha1sum(env.files.disk)
    if disk_sha1 != env.files.disk_sha1:
        raise ValueError(
            f"Original binary file `{env.files.disk}` has SHA-1 `{disk_sha1}`, "
            f"expected `{env.files.disk_sha1}`"
        )

    if not env.files.disk_rom.is_file():
        LOG.info("Creating ROM file for comparison: `%s`", str(env.files.disk_rom))
        subprocess.run(
            f"{env.generate_objcopy_rom_cmd('0x4FBD00')} {env.files.disk} {env.files.disk_rom}",
            shell=True,
            check=True,
        )


def split(env: Environment, clean_first: bool = False):
    """
    Split and disassemble the target binary.

    If `clean_first` is `True`, the environment will be cleaned of build artifacts
    and previous splits/disassembly before splitting.
    """

    if clean_first:
        LOG.info("Cleaning the repository before splitting binary.")
        distclean(env)

    _prepare_rom(env)

    LOG.info("Invoking `splat` to split the binary.")
    splat_split.main(config_path=[env.files.splat_yaml], modes="all", verbose=False)

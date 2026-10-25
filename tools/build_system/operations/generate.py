"""
Implementation of the "generate" command.
"""

import argparse
import json
import logging
import shlex
from pathlib import Path

import splat.scripts.split as splat_split
import splat.segtypes.common as splat_segtypes
from splat.segtypes.linker_entry import LinkerEntry

from build_system import ninja_syntax
from build_system.env import Environment
from .clean import clean
from .split import split

LOG = logging.getLogger(__name__)


def add_subparser(subparsers):
    """
    Add the argument parser for the command to the action object for
    the main parser.
    """
    parser: argparse.ArgumentParser = subparsers.add_parser(
        "generate",
        help="Generates the Ninja build script and `objdiff` config for the project.",
        description="Generates the Ninja build script and `objdiff` config for the project.",
    )
    parser.set_defaults(func=_generate_cli)


def _generate_cli(env: Environment, args):
    """
    Handle the "generate" command of the CLI.
    """
    generate(env)
    LOG.info("Done generating build script.")
    LOG.info("")
    LOG.info("Use `ninja` at the root of the repository to build the project.")


def _generate_ninja_script(env: Environment, linker_entries: list[LinkerEntry]):
    """
    Generate the Ninja build script.
    """

    # Regardless of flags, remove any existing build script.
    env.files.build_script.unlink(missing_ok=True)

    LOG.debug("C Compiler Command:    %s", env.generate_c_compiler_cmd())
    LOG.debug("CXX Compiler Command:  %s", env.generate_cxx_compiler_cmd())
    LOG.debug("AS Command:            %s", env.generate_assembler_cmd())
    LOG.debug("LD Command:            %s", env.generate_linker_cmd())

    objdiff_units: list[dict] = []
    objdiff_categories: set[str] = set()
    compile_commands: list[dict] = []

    with env.files.build_script.open("w") as f:
        ninja = ninja_syntax.Writer(f)

        # Create rules for ninja build steps.
        ninja.rule(
            name="cc",
            description="CC          $in",
            command=f"{env.generate_c_compiler_cmd()} $in -o $out && {env.toolchain.strip_cmd} $out -N dummy-symbol-name",
        )
        ninja.rule(
            name="cxx",
            description="CXX         $in",
            command=f"{env.generate_cxx_compiler_cmd()} $in -o $out && {env.toolchain.strip_cmd} $out -N dummy-symbol-name",
        )
        ninja.rule(
            name="as",
            description="ASSEMBLE    $in",
            command=f"{env.generate_assembler_cmd()} $in -o $out && uv run configure.py patchelf $out",
        )
        ninja.rule(
            name="ld",
            description="LINK        $in",
            command=f"{env.generate_linker_cmd()} -T $in -o $out",
        )
        ninja.rule(
            name="rom",
            description="ROM         $in",
            command=f"{env.generate_objcopy_rom_cmd('0x3FBD00')} $in $out",
        )
        ninja.rule(
            name="check",
            description="CHECK       $in",
            command="uv run configure.py check --binary $in --output $out",
        )
        ninja.rule(
            name="progress",
            description="PROGRESS    $in",
            command=f"{env.files.objdiff_cli} report generate -o {env.files.progress_report} -f json-pretty",
        )

        # The environment uses absolute paths, but we want to use relative paths
        # for the Ninja script. This is to allow `objdiff` to auto-build
        # objects after any source changes.
        local_final_elf = env.files.final_elf.relative_to(env.directories.root)
        local_final_rom = env.files.final_rom.relative_to(env.directories.root)
        local_ldscript = env.files.ldscript.relative_to(env.directories.root)
        local_ldmap = env.files.ldmap.relative_to(env.directories.root)
        local_build_dir = env.directories.build.relative_to(env.directories.root)
        local_objdiff_config = env.files.objdiff_config.relative_to(
            env.directories.root
        )

        linked_objects: set[Path] = set()
        # Add each object to the build script according to its segment type.
        for entry in linker_entries:
            segment = entry.segment

            # Any uncategorized TUs are likely data-only.
            cat_split = segment.name.split("/")
            category: str = cat_split[0] if len(cat_split) > 1 else "data"
            objdiff_categories.add(category)

            # Create the `objdiff` unit for this object.
            unit: dict = {}
            unit["name"] = segment.name
            unit["target_path"] = str(entry.object_path)
            unit["base_path"] = None
            unit["metadata"] = {}
            unit["metadata"]["progress_categories"] = [category]

            if _is_assemblable(segment):
                # Incomplete TU with no decompiled equivalent.
                _add_build_rule(ninja, entry.object_path, "as", entry.src_paths)
            else:
                # In-progress or fully decompiled TU which can be compared with the original.
                if _is_c_code(segment):
                    rule = "cc"
                    compiler_cmd = env.generate_c_compiler_cmd()
                elif _is_cxx_code(segment):
                    rule = "cxx"
                    compiler_cmd = env.generate_cxx_compiler_cmd()
                else:
                    raise AssertionError(f"Unknown segment type {type(segment)}!")
                _add_build_rule(ninja, entry.object_path, rule, entry.src_paths)

                # `ninja -t compdb` would include the chained `strip` command, so
                # write the compile commands for IDEs ourselves.
                compile_commands.append(
                    {
                        "directory": str(env.directories.root),
                        "arguments": [
                            *shlex.split(compiler_cmd),
                            *[str(p) for p in entry.src_paths],
                            "-o",
                            str(entry.object_path),
                        ],
                        "file": str(entry.src_paths[0]),
                        "output": str(entry.object_path),
                    }
                )

                # We also want to build the original (now-unused) assembly file
                # so we can compare our decompiled result using `objdiff`.
                local_asm_out_path = segment.asm_out_path().relative_to(
                    env.directories.root
                )
                local_out_path = segment.out_path().relative_to(env.directories.root)
                target_path = (local_build_dir / local_asm_out_path).with_suffix(".s.o")
                _add_build_rule(ninja, target_path, "as", [local_asm_out_path])

                unit["target_path"] = str(target_path)
                unit["base_path"] = str(entry.object_path)
                unit["metadata"]["source_path"] = str(local_out_path)

            if entry.object_path.suffix == ".o":
                linked_objects.add(entry.object_path)

            objdiff_units.append(unit)

        # Add the linker step.
        ninja.build(
            outputs=[str(local_final_elf)],
            rule="ld",
            inputs=[str(local_ldscript)],
            implicit=[str(obj) for obj in linked_objects],
            implicit_outputs=[str(local_ldmap)],
        )

        # Add the final object step.
        ninja.build(
            outputs=[str(local_final_rom)],
            rule="rom",
            inputs=[str(local_final_elf)],
        )

        # Add a step to ensure the new binary matches.
        ninja.build(
            outputs=[str(local_build_dir / "build.sha1")],
            rule="check",
            inputs=[str(local_final_rom)],
        )

        # Add a step to print binary progress if matching.
        ninja.build(
            outputs=[str(local_build_dir / "progress.txt")],
            rule="progress",
            inputs=[str(local_objdiff_config)],
            implicit=[str(local_build_dir / "build.sha1")],
        )

    # Complete our `objdiff` configuration.
    objdiff_config: dict = {
        "$schema": "https://raw.githubusercontent.com/encounter/objdiff/main/config.schema.json",
        "custom_make": "ninja",
        "custom_args": [],
        "build_target": True,
        "build_base": True,
        "watch_patterns": [],
        "units": objdiff_units,
        "progress_categories": [{"id": c, "name": c} for c in objdiff_categories],
        "options": {
            # GCC 2.9 uses the legacy GNU (v2) C++ name mangling scheme.
            "demangler": "gnu_legacy",
        },
    }
    with env.files.objdiff_config.open(mode="w") as f:
        json.dump(objdiff_config, f, indent=2)

    LOG.info("Generating `compile_commands.json` for IDEs.")
    target = env.directories.root / "compile_commands.json"
    with target.open(mode="w") as f:
        json.dump(compile_commands, f, indent=2)


def generate(env: Environment, clean_first: bool = True):
    """
    Generate the Ninja build script and `objdiff` config for the project.
    """
    if clean_first:
        LOG.info("Cleaning build artifacts.")
        clean(env)

    # If we can't detect that we've previously split the binary, distclean the repo first
    # to make sure we're in a good state before trying to build.
    split(env, clean_first=not env.previously_split())

    # Retrieve the linker entries generated by `splat`.
    linker_entries: list[LinkerEntry] = [
        entry
        for entry in splat_split.linker_writer.entries
        if entry.segment.type[0] != "." and entry.object_path is not None
    ]

    _generate_ninja_script(env, linker_entries)


def _add_build_rule(
    ninja: ninja_syntax.Writer, output: Path, rule: str, inputs: list[Path]
):
    """
    Add a build with the given output and inputs to the Ninja script.
    """
    ninja.build(outputs=[str(output)], rule=rule, inputs=[str(p) for p in inputs])


def _is_assemblable(segment: splat_segtypes.segment.Segment) -> bool:
    """
    Determine if the `splat` segment should be fed to the assembler directly.
    """
    assemblable_types = (
        splat_segtypes.asm.CommonSegAsm,
        splat_segtypes.data.CommonSegData,
        splat_segtypes.databin.CommonSegDatabin,
        splat_segtypes.rodatabin.CommonSegRodatabin,
    )
    return isinstance(segment, assemblable_types)


def _is_c_code(segment: splat_segtypes.segment.Segment) -> bool:
    """
    Determine if the `splat` segment should be fed to the C compiler.
    """
    return isinstance(segment, splat_segtypes.c.CommonSegC) and not isinstance(
        segment, splat_segtypes.cpp.CommonSegCpp
    )


def _is_cxx_code(segment: splat_segtypes.segment.Segment) -> bool:
    """
    Determine if the `splat` segment should be fed to the C++ compiler.
    """
    return isinstance(segment, splat_segtypes.cpp.CommonSegCpp)

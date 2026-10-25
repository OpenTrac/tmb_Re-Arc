# Twisted Metal: Black Re-Arc Project

This repository is a Re-Arc (Reverse Engineering, Archaeology, and Reconstruction of Code)
project for the 2001 Playstation 2 game `Twisted Metal: Black`. It uses matching decompilation
techniques to ensure that the reconstructed source produces exactly identical machine code and
behavior versus the original.

```diff
- WARNING! -

This repository is a work in progress, and while it can be used to make certain changes, it's
still constantly evolving. If you wish to use it for modding purposes in its current state,
please be aware that the codebase could drastically change at any time.
```

```diff
- WARNING! -

The project maintainer sincerely requests that others do not attempt to port this
repository's code to other platforms until the project nears completion.

Upon the project's completion, if a source port to other platforms is desired, a
unified, non-profit effort to create an open-source engine replacement should be
started to avoid any community fragmentation.
```

## AI Policy

Before using agentic LLMs or AI on this codebase, please see [AI_POLICY.md](AI_POLICY.md)
for our repository's standards and requirements.

## Contributing

If you'd like to contribute to the project, please join us in the `#twisted-metal-black`
channel on the [PS1/PS2 Re-Arc Discord server](https://discord.gg/VwCPdfbxgm).
Communication is key.

To learn how to contribute to this decompilation project, please read [CONTRIBUTING](CONTRIBUTING.md).

## Requirements

This repository currently assumes that the user is running a Debian/Ubuntu-based
Linux distribution. If you are a Windows user, you should use
[Windows Subsystem for Linux](https://learn.microsoft.com/en-us/windows/wsl/about)
with Ubuntu.

### System Packages

This repository requires several system packages which can be found in the
`tools/requirements-debian.txt` file. To install all required system packages,
run the following command from the root of the repository:

```sh
sudo apt update
sudo apt install -y $(cat ./tools/requirements-debian.txt)
```

### Python Packages

This repository manages its Python version and dependencies with
[`uv`](https://docs.astral.sh/uv/). Follow the
[installation instructions](https://docs.astral.sh/uv/getting-started/installation/)
to install it.

A script in the repository, `tools/setup_dev.sh`, will automatically perform first-time
setup for all Python dependencies and install the `pre-commit` hooks.

```sh
./tools/setup_dev.sh
```

There is no need to activate a virtual environment. Run Python scripts in the repository
through `uv run` instead, e.g. `uv run configure.py generate`.

## Building

To build the repository, you will need a digital copy of the Playstation 2 game
`Twisted Metal: Black (NTSC) (SCUS-97101)`.

1. Setup your environment as defined by the [Requirements](#requirements) section.
2. Extract the executable binary ELF file `SCUS_971.01` from the Playstation 2 ISO file.
  - In the future, this repository will be able to perform this extraction for you.
    Currently, however, you will need to somehow extract this yourself.
3. Place the `SCUS_971.01` file into the `disks/` directory of the repository.
4. Run `uv run configure.py generate` at the root of the repository.
  - This will split the binary into parts and generate a `build.ninja` file in the
    root of the repository.
5. Run `ninja` at the root of the repository.
  - If building is successful, the resulting binary will appear at `build/SCUS_971.01`.
6. Whenever you modify the splat config, you should perform a `uv run configure.py distclean`
   before regenerating the config with `uv run configure.py generate`.

## Known Issues / Compiler Quirks

For a list of known compiler issues / quirks to reference during the decompilation
process, check `QUIRKS.md`.

## Disclaimers / Legality

This project may fall under a legal grey zone of copyright law in some locations.

This project does not and will never reference or incorporate any leaked Sony implementation
source code, and contributions which do so will not be accepted. The authors of this project
make no claim of copyright on any Sony proprietary APIs or structures.

This project exists primarily for educational and research purposes. The authors of this
project have no desire to use this project for monetary gain.

## Acknowledgements

This project is critically reliant on tools from dedicated members of the console
reversing community. Without them, this work would be impossible.
Special thanks go to the teams and individuals behind:

- `decomp.wiki`
- `decomp.me`
- `decomp.dev`
- `splat`
- `spimdisasm`
- `objdiff` and `asm-differ`
- `m2c`
- All of the helpful and skilled decompers in various decomp communities and
  Discords; PS1/PS2, GC/Wii, N64, and decomp.me servers.

Also, SingleTrac/Incognito, for developing Twisted Metal, Jet Moto, and all of the other
great games they put out during the 90s and 2000s. This project's goal is to preserve
that legacy for future generations and allow others to experience what we did.

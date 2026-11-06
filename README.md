# ENCORE. VST

Free audio plugins from ENCORE., for any DAW that loads VST3 (and LV2 on Linux):

| Plugin | What it is |
|---|---|
| **Vox** | All-in-one vocal chain: clean (low cut, gate, de-ess), tune, harmony, rider, tone, dynamics, space |
| **Verb** | Algorithmic reverb: Room, Chamber, Plate, Hall, Huge (with shimmer), Spring; ducking, movement, width |
| **Equalize** | Manual 8-band parametric EQ with live spectrum, plus a Smart tab: Carve (side-chain ducking EQ) and Smart bands (dynamic cuts, with a resonance finder) |
| **Master** | One-click mastering: 8 styles, Listen learns the song's level and tone, true-peak limiter, LUFS targets, loudness-matched A/B |
| **Strum** | Acoustic guitar fixer: Body (pickup to mic: Learn from a mic on the side-chain, or a generic curve), Boom, Squeak, Pick, Double, loudness-matched A/B |

Every plugin has factory presets plus user presets
(`Documents\Encore\<Plugin>\Presets\*.encorepreset` on Windows). The same
plugins are built into ENCORE. Basement Studio.

Copyright (c) Encore Music, Inc. Licensed under the GNU Affero General Public
License v3 (see `LICENSE`). The ENCORE. name and logo are not licensed (see
`TRADEMARKS.md`). Third-party code: `THIRD-PARTY-NOTICES.md`. Outside
contributions are not accepted yet (see `CONTRIBUTING.md`).
Contact: support@encoremusic.studio

## Downloads

Each release (under Releases on this repo) has:

- `ENCORE-VST-<version>-windows-setup.exe`: ENCORE. VST Setup for Windows 10 and
  11 (64-bit). Tick the plugins you want. It isn't code-signed yet, so
  SmartScreen may warn you: click **More info**, then **Run anyway**. To add or
  remove plugins later, run it again or use **Modify** on ENCORE. VST in
  Windows Settings > Apps. Your presets are never touched.
- `ENCORE-VST-<version>-linux-x64.tar.gz` and `ENCORE-VST-<version>-linux-arm64.tar.gz`:
  VST3 and LV2 builds for 64-bit Linux on PCs and on Raspberry Pi (ENCORE. OS, or
  64-bit Raspberry Pi OS on a Pi 4 or 5). Copy the `.vst3` folders to `~/.vst3`
  and the `.lv2` folders to `~/.lv2`.
- A `.sha256` file next to each download.

After installing, rescan plugins in your DAW (Cubase: Studio > VST Plug-in
Manager, then Rescan).

## What is here

```
plugins/<Name>/                     one folder per plugin (Vox, Verb, EQ, Master, Strum)
shared/                             what the plugins share: EncoreProcessorBase.h, and ui/
shared/ui/                          controls, presets (factory and user), the ENCORE. theme
shared/ui/fonts/                    DM Sans and Space Mono (SIL OFL), built into every editor
installer/                          ENCORE. VST Setup, the Windows installer (package.ps1 builds it)
tests/                              plugin-level tests: presets, installer, editor screenshots
external/JUCE                       JUCE 9 (submodule)
external/encore-audio-processing    ENCORE.'s audio processing headers (submodule)
build-and-install.ps1               Windows: build, test and install the VST3s on this PC
Install Encore Plugins.cmd          double-click version of build-and-install.ps1
```

## Submodules and the include path

Two submodules, both pinned:

| Path | What | Pinned to |
|---|---|---|
| `external/JUCE` | JUCE 9 (bundles the VST3 SDK 3.8, MIT) | `7ff644f` |
| `external/encore-audio-processing` | the audio processing headers (`dsp/...`) | tag `v1.0.0` |

`CMakeLists.txt` puts `shared/` and `external/encore-audio-processing` on every
target's include path, so `#include "ui/Theme.h"` and `#include "dsp/MasterChain.h"`
both resolve.

Initialise the submodules without `--recursive`. encore-audio-processing has
its own `external/JUCE`, which is only for its own tests:

```bash
git clone https://github.com/Encore-Music-Inc/encore-vst.git
cd encore-vst
git submodule update --init external/JUCE external/encore-audio-processing
```

To move to a new encore-audio-processing release, check out its tag in the
submodule and commit the change in a pull request:

```bash
git -C external/encore-audio-processing fetch --tags
git -C external/encore-audio-processing checkout vX.Y.Z
git add external/encore-audio-processing
```

## Building

Needs CMake 3.22 or newer and a C++20 compiler.

### Windows

Visual Studio 2022 (or its Build Tools) with the C++ workload.

```powershell
.\build-and-install.ps1   # builds, runs the tests, installs the VST3s (asks for admin)
```

Or double-click `Install Encore Plugins.cmd` (close Cubase first). It also
removes old "Sick & Sad" copies of the plugins so Cubase doesn't list each one
twice (the plugin IDs didn't change, so old projects open with the new names).

To build the installer, `dist\ENCORE-VST-<version>-windows-setup.exe`:

```powershell
powershell -ExecutionPolicy Bypass -File .\installer\package.ps1
```

It builds every plugin, runs the tests, draws each plugin's editor for its card
in the installer, zips the plugins into the installer's payload, then builds
ENCORE. VST Setup and runs its own tests (SetupTests). The installer's CMake
targets (`EncoreVstSetup`, `EncoreVstSetupDev`, `SetupTests`) only appear once
that payload exists.

### Linux and Raspberry Pi (builds VST3 and LV2)

```bash
sudo apt install build-essential cmake ninja-build libasound2-dev libjack-jackd2-dev \
  libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev \
  libxext-dev libxi-dev libxinerama-dev libxrandr-dev libxrender-dev \
  libglu1-mesa-dev mesa-common-dev
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### macOS (builds VST3 and AU; not released yet)

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
cmake --build build
```

The plugins land in `build/plugins/<Name>/<Target>_artefacts/Release/`
(`VST3/`, `LV2/`, `AU/`, `Standalone/`).

## Tests

```bash
ctest --test-dir build --output-on-failure            # Windows: add -C Release
```

On Linux without a display, run it under `xvfb-run -a`: StrumTests and the
editor shots open the plugins' editors.

| Test | What it checks |
|---|---|
| `PresetTests` | the user preset system, round trip |
| `StrumTests` | Strum's audio processing and the whole plugin (Learn, state, presets). The source is `external/encore-audio-processing/tests/StrumTests.cpp`, built here with `ENCORE_STRUM_PLUGIN_TESTS=1` and the plugin's own processor |
| `ShotVox` ... `ShotStrum` | each plugin's editor draws (to `build/shots/<Name>.png`) |
| `SetupTests` | the installer: install, replace, remove, in use, the Windows entry (Windows, after `installer/package.ps1`) |

The other audio processing suites (pitch, reverb, EQ, mastering, the build
without JUCE) live in encore-audio-processing and run there.

## CI

`.github/workflows/ci.yml`:

- Pull requests and pushes to `main`: Linux x64 build of all five plugins and the tests.
- `v*` tags and manual runs: also Linux arm64, and Windows x64 (the plugins,
  the tests and the installer).
- A pull request or push that only changes `*.md`, `LICENSE` or
  `.github/CODEOWNERS` skips the builds (skipped counts as passing).
- While encore-audio-processing is private, CI needs a repository secret
  `ENCORE_AUDIO_PROCESSING_TOKEN`: a token with read access to that repo's
  contents, used to check out the submodule.

## Releases

Versions are `vMAJOR.MINOR.PATCH`. The version lives in `CMakeLists.txt`
(`project(EncorePlugins VERSION ...)`) and is built into the installer, so
change it on `main` (through a pull request) before releasing. CI refuses a
tag that doesn't match it.

A person at Encore Music, Inc. creates each release in GitHub: Releases, Draft
a new release, choose a new tag `vX.Y.Z` on `main`, Publish. The tag starts CI,
which builds and tests everything and attaches the Windows installer, the Linux
x64 and arm64 archives and their `.sha256` files to the Release.

Version 0.4.0 and earlier were published from a different repository; their
release is kept here as it was.

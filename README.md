# ENCORE. VST

Free audio plugins from ENCORE., for any DAW that loads VST3, Audio Units (Mac) or LV2 (Linux):

| Plugin | What it is |
|---|---|
| **Vox** | All-in-one vocal chain: clean (low cut, gate, de-ess), tune, harmony, rider, tone, dynamics, space |
| **Verb** | Algorithmic reverb: Room, Chamber, Plate, Hall, Huge (with shimmer), Spring; ducking, movement, width |
| **Equalize** | Manual 8-band parametric EQ with live spectrum, plus a Smart tab: Carve (side-chain ducking EQ) and Smart bands (dynamic cuts, with a resonance finder) |
| **Master** | One-click mastering: 8 styles, Listen learns the song's level and tone, true-peak limiter, LUFS targets, loudness-matched A/B |
| **Strum** | Acoustic guitar fixer: Body (pickup to mic: Learn from a mic on the side-chain, or a generic curve), Boom, Squeak, Pick, Double, loudness-matched A/B |

Every plugin has factory presets plus user presets
(`Documents\ENCORE\<Plugin>\Presets\*.encorepreset` on Windows,
`~/Documents/ENCORE/<Plugin>/Presets` on a Mac). The same
plugins are built into ENCORE. Basement Studio.

Copyright (c) Encore Music, Inc. Licensed under the GNU Affero General Public
License v3 (see `LICENSE`). The ENCORE. name and logo are not licensed (see
`TRADEMARKS.md`). Third-party code: `THIRD-PARTY-NOTICES.md`. Outside
contributions are not accepted (see `CONTRIBUTING.md`).
Contact: support@encoremusic.studio

## Downloads

Each release (under Releases on this repo) has:

- `ENCORE-VST-<version>-windows-setup.exe`: ENCORE. VST Setup for Windows 10 and
  11 (64-bit). Tick the plugins you want. It isn't code-signed, so
  SmartScreen may warn you: click **More info**, then **Run anyway**. To add or
  remove plugins later, run it again or use **Modify** on ENCORE. VST in
  Windows Settings > Apps. **Uninstall** there removes all of them. Your presets
  are never touched.
- `ENCORE-VST-<version>-windows-x64-vst3.zip`: the same VST3 plugins without the
  installer, to copy to `C:\Program Files\Common Files\VST3` yourself.
- The Windows plugins are 64-bit Intel/AMD (x64). On a Windows on Arm PC
  (Snapdragon), use your DAW's x64 version to load them: a DAW running natively
  on Arm can't load x64 plugins.
- `ENCORE-VST-<version>-macos.pkg`: the Mac installer, for Apple Silicon and
  Intel Macs on macOS 11 or newer. It installs VST3 and Audio Units (for Logic
  and GarageBand); **Customize** lets you untick plugins. It isn't signed or
  notarized, so the first time macOS says it was "Not Opened": click
  **Done**, go to **System Settings > Privacy & Security**, scroll to Security
  and click **Open Anyway**, then open the installer again and click **Open
  Anyway**. To remove a plugin, delete it from `/Library/Audio/Plug-Ins/VST3`
  and `/Library/Audio/Plug-Ins/Components`.
- `ENCORE-VST-<version>-macos.zip`: the same Mac plugins without the installer,
  to copy to `/Library/Audio/Plug-Ins/VST3` and `/Library/Audio/Plug-Ins/Components`
  yourself.
- `ENCORE-VST-<version>-linux-x64.tar.gz` and `ENCORE-VST-<version>-linux-arm64.tar.gz`:
  VST3 and LV2 builds for 64-bit Linux on PCs and on Raspberry Pi (ENCORE. OS, or
  64-bit Raspberry Pi OS on a Pi 4 or 5). They run on Raspberry Pi OS Bookworm or
  newer, Debian 12 or newer and Ubuntu 22.04 or newer. Copy the `.vst3` folders to
  `~/.vst3` and the `.lv2` folders to `~/.lv2`. To remove a plugin, delete its
  folder from both.
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
installer/macos/package.sh          the Mac installer (.pkg) and zip
tests/                              plugin-level tests: presets, installer, editor screenshots
external/encore-audio-processing    ENCORE.'s audio processing headers (submodule), and the JUCE it pins
build-and-install.ps1               Windows: build, test and install the VST3s on this PC
Install ENCORE Plugins.cmd          double-click version of build-and-install.ps1
```

## Pinned releases

One pin: encore-audio-processing, at a published release. Its JUCE is the JUCE
version for all of ENCORE., so the plugins build with the JUCE its tests run
on. CI checks that each row is a release and is exactly what the folder is
pinned to (`.github/pins/check.sh`).

<!-- pinned-releases:start -->
| Folder | Repository | Release |
|---|---|---|
| `external/encore-audio-processing` | [encore-audio-processing](https://github.com/Encore-Music-Inc/encore-audio-processing) | `v1.0.1` |
| `external/encore-audio-processing/external/JUCE` | [JUCE](https://github.com/juce-framework/JUCE), pinned by encore-audio-processing (shown, not pinned here; bundles the VST3 SDK, MIT) | `9.0.5` |
<!-- pinned-releases:end -->

`CMakeLists.txt` puts `shared/` and `external/encore-audio-processing` on every
target's include path, so `#include "ui/Theme.h"` and `#include "dsp/MasterChain.h"`
both resolve, and builds JUCE from `external/encore-audio-processing/external/JUCE`.

Fetch the two folders by path, never with `--recursive`:

```bash
git clone https://github.com/Encore-Music-Inc/encore-vst.git
cd encore-vst
git submodule update --init --depth 1 external/encore-audio-processing
git -C external/encore-audio-processing submodule update --init --depth 1 external/JUCE
```

To use a new encore-audio-processing release (and the JUCE it pins), check
out its tag in the submodule, update the table above, and commit both in a
pull request titled "Use encore-audio-processing vX.Y.Z (was vA.B.C)":

```bash
git -C external/encore-audio-processing fetch --depth 1 origin tag vX.Y.Z
git -C external/encore-audio-processing checkout vX.Y.Z
git -C external/encore-audio-processing submodule update --init --depth 1 external/JUCE
git add external/encore-audio-processing README.md
```

## Building

Needs CMake (the minimum is in `CMakeLists.txt`) and a C++20 compiler.

### Windows

Visual Studio 2022 (or its Build Tools) with the C++ workload.

```powershell
.\build-and-install.ps1   # builds, runs the tests, installs the VST3s (asks for admin)
```

Or double-click `Install ENCORE Plugins.cmd` (close Cubase first).

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

### macOS (builds VST3 and AU)

Xcode (or its Command Line Tools) and Ninja (`brew install ninja`).

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
cmake --build build
installer/macos/package.sh <version>   # dist/ENCORE-VST-<version>-macos.pkg and .zip
```

`package.sh` checks every plugin is universal (Apple Silicon and Intel), signs
them ad hoc (Apple Silicon won't load unsigned plugins) and builds the
installer and the zip. They aren't signed with a Developer ID or notarized.

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

CI also runs [pluginval](https://github.com/Tracktion/pluginval), Tracktion's
plugin validator, on every plugin: it loads each one the way a DAW does and
checks state save and restore, parameters, odd buffer sizes and sample rates,
opening and closing the editor, and threading. To run it yourself, download
pluginval and run `tests/pluginval.sh <pluginval> 5 <plugin>...` (on Linux
without a display, under `xvfb-run -a`).

## CI

`.github/workflows/ci.yml`. Each push starts one run (push for `main` and `v*`
tags, plus pull requests, manual runs and a scheduled run).

| When | What runs |
|---|---|
| Pull request | in parallel: Linux x64 (VST3), Windows x64 (VST3) and macOS on Apple Silicon (VST3 and AU): the build, the tests, and pluginval on every plugin. No installers or zips |
| Push to `main`, or a manual run | in parallel, the release files: Windows x64 (VST3, ENCORE. VST Setup), macOS universal (VST3, AU, the .pkg and zip; the .pkg is installed on the runner and each Audio Unit checked with `auval`), Linux x64 and Linux arm64 (VST3, LV2). Each with the tests and pluginval. The files are kept as workflow artifacts |
| On the schedule in `ci.yml`, or a manual run with `checks: weekly` | the pull request checks with pluginval at its strictest, and the tests built with AddressSanitizer and UndefinedBehaviorSanitizer on Linux x64 (memory errors and undefined behaviour). Undefined behaviour is reported and the tests carry on, so one run lists every finding; a memory error stops it. Exceptions, each with its reason, go in `.github/sanitizers/ubsan.supp` (none today). A failure shows as a red run under Actions |
| `v*` tag | no build: the files from the successful `main` build of the tag's commit are attached to the Release |

- A pull request or push that only changes `*.md`, `LICENSE` or
  `.github/CODEOWNERS` skips the builds (skipped counts as passing).
- Linux builds inside Debian 12 (Bookworm), the oldest system the plugins
  support, so they load on everything newer too; `.github/linux-compat.sh` fails
  the build if a plugin needs a newer glibc or libstdc++ than Debian 12 has. The
  x64 plugins are then loaded with pluginval in a plain Ubuntu 22.04 as well.
  Linux arm64 builds on GitHub's native arm64 runner.
- Pull requests build the Mac plugins for Apple Silicon only; `main` builds
  them universal.
- Every action is pinned to a commit, with its version in a comment.
  Dependabot proposes updates (`.github/dependabot.yml`).
- The editor screenshots and pluginval logs of every run are kept as the
  `checks-*` artifacts.

### Build speed

- Ninja on every platform, all cores.
- A compiler cache: ccache on Linux and macOS, sccache on Windows, kept between runs with
  `actions/cache`, keyed on OS, architecture, compiler and the JUCE pin.
- JUCE is compiled once per plugin, inside each plugin's shared-code library
  (`juce_add_plugin` does that). The tests and editor shots link those
  libraries instead of compiling JUCE again, so a full build compiles JUCE
  once per plugin, not once per target. One JUCE library shared by all five plugins isn't
  possible with JUCE's CMake: `juce_add_plugin` links `juce_audio_plugin_client`
  into each plugin, which brings every JUCE module's sources with it, and some
  modules read the plugin's own settings (`JucePlugin_Name`, `JucePlugin_Build_*`).
- No full LTO. `-DENCORE_LTO=THIN` turns on ThinLTO (Clang) or incremental LTCG
  (MSVC); it is off. Turn it on only if `MasterBench` (built with
  `-DENCORE_VST_BENCH=ON`; it runs Master on a fixed test file made by
  `MasterBench --make bench.wav`) shows Master using less CPU with it.
- Submodules are checked out at depth 1, never recursively.

## Releases

Versions are `vMAJOR.MINOR.PATCH`. The version lives in `CMakeLists.txt`
(`project(EncorePlugins VERSION ...)`) and is built into the installer, so
change it on `main` (through a pull request, with the release notes in
`CHANGELOG.md`) before releasing. CI refuses a tag that doesn't match it.

Once the `main` build of that commit has passed, a person at Encore Music,
Inc. publishes the release in GitHub (the steps are in `CLAUDE.md`). The tag
starts CI, which doesn't rebuild: it takes the files from the successful
`main` build of that exact commit (the Windows installer and VST3 zip, the Mac
.pkg and zip, the Linux x64 and arm64 archives, each with a `.sha256` file),
attaches them to the Release, and sets the title and notes from
`CHANGELOG.md`. If that commit has no successful `main` build (it's still
running, or the push only changed docs and skipped the builds), the release
job fails and says so: run CI by hand on `main` (Actions, CI, Run workflow),
then re-run the release job. Workflow artifacts expire, so release a commit
while its build's files are still there.

Versions before 0.5.0 were published from a different repository.

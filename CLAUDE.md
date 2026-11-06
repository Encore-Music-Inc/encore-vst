# Notes for Claude (and whoever is driving it)

This is ENCORE. VST: the five plugins (Vox, Verb, Equalize, Master, Strum),
their Windows installer and their tests. The notes in sick-and-sad and
encore-studio do not apply here; this file does.

## Decisions (already made, don't revisit)

- Licence: GNU AGPLv3 (`LICENSE`). Copyright holder: Encore Music, Inc.
- The repo is public. Outside contributions are not accepted
  (`CONTRIBUTING.md`). The ENCORE. name and logo are not licensed (`TRADEMARKS.md`).
- Contact: support@encoremusic.studio
- Always write ENCORE. (capitals and the period) in anything people read, and
  "ENCORE. OS" with a space.
- No long dashes in any text. Use commas, periods or parentheses.
- Don't write "DSP" in names, titles or prose for people (in music business it
  means Spotify or Apple Music). Say "audio processing". Code folders and
  identifiers keep the name.
- Anything third-party goes in `THIRD-PARTY-NOTICES.md`.
- The product: VST3 on Windows x64 (with the installer), and VST3 plus LV2 on
  Linux x64 and arm64 (ENCORE. OS on Raspberry Pi).
- Green Room code (sick-and-sad's old `web/`) must never be added here.

## Layout

| Path | What |
|---|---|
| `plugins/<Name>/` | one folder per plugin; the EQ's CMake target is `SnsEQ` |
| `shared/` | `EncoreProcessorBase.h` and `ui/` (controls, presets, theme, `fonts/`) |
| `installer/` | ENCORE. VST Setup (Windows) and `package.ps1`, which builds it |
| `tests/` | PresetTests, SetupTests, EditorShot (the Shot<Name> targets), MasterBench (Master's CPU use, `-DENCORE_VST_BENCH=ON`) |
| `external/JUCE` | JUCE 9 submodule pinned at `7ff644f` |
| `external/encore-audio-processing` | the audio processing headers, submodule pinned at tag `v1.0.0` |
| `build-and-install.ps1`, `Install Encore Plugins.cmd` | Windows: build, test and install locally |
| `.github/workflows/ci.yml` | pull requests: Linux x64 (VST3) and tests; main and manual runs: Windows x64 with the installer, Linux x64 and arm64, as artifacts; `v*` tags: attach main's files to the Release, no rebuild; "CI passed" is the one check to require |

## Submodules

- Never initialise them recursively. Use
  `git submodule update --init external/JUCE external/encore-audio-processing`.
  encore-audio-processing's own `external/JUCE` is for its tests only.
- `shared/` and `external/encore-audio-processing` are on the include path, so
  `#include "dsp/MasterChain.h"` resolves. Don't copy the audio processing
  headers back into this repo: change them in encore-audio-processing, release
  it, then move the pin here in a pull request.
- `StrumTests` is built from `external/encore-audio-processing/tests/StrumTests.cpp`
  with `ENCORE_STRUM_PLUGIN_TESTS=1`. The other audio processing suites run in
  that repo, not here.

## Building and testing

- Keep builds fast: Ninja, ccache/sccache in CI, no full LTO (`ENCORE_LTO` is
  OFF; THIN is ThinLTO/incremental LTCG), and tests link the plugins' own
  libraries (`encore_plugin_test` in `CMakeLists.txt`) instead of compiling JUCE again.
- Linux: install the packages listed in the README (JUCE 9 needs `libxi-dev`),
  then `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`, `cmake --build build`,
  `xvfb-run -a ctest --test-dir build --output-on-failure`. All must pass before a
  pull request.
- Windows: `build-and-install.ps1`; the installer: `installer/package.ps1`.

## Versioning and releases

- Semantic versioning, tags `vMAJOR.MINOR.PATCH`. MAJOR for anything that breaks
  people's saved work (a changed plugin ID or parameter, or presets that no longer
  load); MINOR for new features; PATCH for fixes.

## Release process

1. The version lives in exactly one place: `project(EncorePlugins VERSION x.y.z)`
   in `CMakeLists.txt`.
2. Changing it is its own small pull request ("Release vX.Y.Z"), so a person sees the
   number before it's built.
3. Merging builds everything on `main`: the tests run, and the Windows installer and
   VST3 zip and the Linux x64 and arm64 archives are uploaded as workflow artifacts.
4. A person publishes the release in GitHub (Releases, Draft a new release, a new tag
   `vX.Y.Z` on that `main` commit, Publish). Never a Claude session.
5. The tag's CI run doesn't rebuild. It checks that the tag matches the version, finds
   the successful `main` build of that exact commit and attaches its files, each with a
   `.sha256` file, to the Release. If either check fails, nothing is attached.
   If that commit was docs-only (its build was skipped), run CI by hand on `main`
   (Actions, CI, Run workflow) and then re-run the release job.

- Claude sessions can't push tags: the push is refused with HTTP 403. Don't try, and
  don't look for another way (the API, a workflow, a different remote). When a
  release is due, open the version pull request, then tell your human the version
  number, why that number, and what changed since the last release, and wait for
  them to publish it.
- Tags `v*` are protected: they can't be moved or deleted (an org admin can, in an
  emergency).

## Changes

- main only changes through pull requests. Never push to main directly.

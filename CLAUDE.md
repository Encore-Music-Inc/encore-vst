# Notes for Claude (and whoever is driving it)

This is ENCORE. VST: the five plugins (Vox, Verb, Equalize, Master, Strum),
their Windows installer and their tests. The notes in sick-and-sad and
encore-studio do not apply here; this file does.

## Decisions (already made, don't revisit)

- Licence: GNU AGPLv3 (`LICENSE`). Copyright holder: Encore Music, Inc.
- The repo will be public. Outside contributions are not accepted
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
| `tests/` | PresetTests, SetupTests, EditorShot (the Shot<Name> targets) |
| `external/JUCE` | JUCE 9 submodule pinned at `7ff644f` |
| `external/encore-audio-processing` | the audio processing headers, submodule pinned at tag `v1.0.0` |
| `build-and-install.ps1`, `Install Encore Plugins.cmd` | Windows: build, test and install locally |
| `.github/workflows/ci.yml` | Linux x64 on pull requests and main; Windows, installer and Linux arm64 on `v*` tags and manual runs; the Release on tags |

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

- Linux: install the packages listed in the README (JUCE 9 needs `libxi-dev`),
  then `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`, `cmake --build build`,
  `xvfb-run -a ctest --test-dir build --output-on-failure`. All must pass before a
  pull request.
- Windows: `build-and-install.ps1`; the installer: `installer/package.ps1`.

## Versioning and releases

- The version is `project(EncorePlugins VERSION ...)` in `CMakeLists.txt`. CI
  fails a tag that doesn't match it.
- Releases are created by a person, never by a Claude session. In GitHub: Releases,
  Draft a new release, choose a new tag `vX.Y.Z` on `main`, Publish. That tag starts
  CI, which builds everything and attaches the installer, the Linux archives and
  their `.sha256` files to the Release.
- Claude sessions can't push tags: the push is refused with HTTP 403. Don't try, and
  don't look for another way (the API, a workflow, a different remote). When a
  release is due, tell your human the version number, why that number, and what
  changed since the last release, then wait for them to publish it.
- Keep the existing v0.4.0 tag and release as they are.

## Changes

- main only changes through pull requests. Never push to main directly.

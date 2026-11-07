# Notes for Claude (and whoever is driving it)

This is ENCORE. VST: the five plugins (Vox, Verb, Equalize, Master, Strum),
their Windows and Mac installers and their tests.

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
- The product: VST3 on Windows x64 (with the installer), VST3 plus Audio Units
  on macOS (universal, with a .pkg installer), and VST3 plus LV2 on Linux x64
  and arm64 (ENCORE. OS on Raspberry Pi).
- Mac builds are signed ad hoc only, not with a Developer ID, and not notarized,
  until ENCORE. has an Apple Developer account. The README tells people how to
  open them (Privacy & Security, Open Anyway).
- Green Room code must never be added here (Green Room is part of the ENCORE. app, in the encoremusic repo).

## Layout

| Path | What |
|---|---|
| `plugins/<Name>/` | one folder per plugin; each folder's CMake target has the same name |
| `shared/` | `EncoreProcessorBase.h` and `ui/` (controls, presets, theme, `fonts/`) |
| `installer/` | ENCORE. VST Setup (Windows) and `package.ps1`, which builds it; `macos/package.sh`, the Mac .pkg and zip |
| `tests/` | PresetTests, SetupTests, EditorShot (the Shot<Name> targets), MasterBench (Master's CPU use, `-DENCORE_VST_BENCH=ON`) |
| `external/encore-audio-processing` | the audio processing headers, pinned to release `v1.0.1`; its `external/JUCE` (release `9.0.5`) is the JUCE the plugins build with |
| `.github/pins/check.sh` | CI's check that every pin is a release (the README's Pinned releases table) |
| `build-and-install.ps1`, `Install ENCORE Plugins.cmd` | Windows: build, test and install locally |
| `.github/workflows/ci.yml` | pull requests: Linux x64, Windows x64 and macOS (Apple Silicon), build, tests and pluginval, no packaging; main and manual runs: Windows x64 with the installer, macOS universal with the .pkg, Linux x64 and arm64, as artifacts; weekly: strictest pluginval and sanitizers; `v*` tags: attach main's files to the Release, no rebuild; "CI passed" is the one check to require |
| `.github/sanitizers/` | the weekly sanitizer run's exceptions (`ubsan.supp`, each with its reason; keep it short) and `report.sh` |
| `tests/pluginval.sh` | runs pluginval on the plugins (CI uses it on Linux and macOS) |

## Submodules

- One pin: encore-audio-processing. JUCE comes from it
  (`external/encore-audio-processing/external/JUCE`, the JUCE version for all
  of ENCORE.); this repo has no JUCE of its own. Never initialise recursively:
  `git submodule update --init --depth 1 external/encore-audio-processing`, then
  `git -C external/encore-audio-processing submodule update --init --depth 1 external/JUCE`.
- `shared/` and `external/encore-audio-processing` are on the include path, so
  `#include "dsp/MasterChain.h"` resolves. Don't copy the audio processing
  headers back into this repo: change them in encore-audio-processing, release
  it, then move the pin here in a pull request.
- `StrumTests` is built from `external/encore-audio-processing/tests/StrumTests.cpp`
  with `ENCORE_STRUM_PLUGIN_TESTS=1`. The other audio processing suites run in
  that repo, not here.

## How ENCORE.'s repositories fit together

```
encore-audio-processing ──▶ JUCE                        (the JUCE version for all of ENCORE.)
encore-vst              ──▶ encore-audio-processing     (brings JUCE with it)
encore-basement-studio  ──▶ encore-vst                  (brings the sound engine and JUCE with it)
                        ──▶ Tracktion Engine
encore-os               ──▶ encore-vst and encore-basement-studio releases
Green Room (encoremusic)──▶ master.wasm from encore-audio-processing, by its SHA-256
```

- Each repository pins each thing it uses in one place, and only to a
  published release. The Pinned releases table in the README names them, and
  CI (`.github/pins/check.sh`) checks that each one is a release and is exactly
  what the folder is pinned to. An exception (no usable release exists) is
  written in the table with its reason.
- Nothing is pinned twice. A repository that uses encore-vst uses encore-vst's
  sound engine and JUCE, never its own copy; one that uses
  encore-audio-processing uses its JUCE.
- Fetch pinned folders by their paths, one level at a time
  (`git -C <folder> submodule update --init --depth 1 <path>`), never with
  `--recursive`: some pinned repositories have test-only folders of their own.
- Changes travel up one repository at a time, each through a release:
  encore-audio-processing, then encore-vst, then encore-basement-studio, then
  encore-os (Green Room beside them). A repository moves to a new release only
  when something changes for its own users, and picks its own version number
  by what its users notice, not by the number of what it uses.
- Pull request titles: "Use <repository> vX.Y.Z (was vA.B.C)" in the repository
  that moves a pin; "Release vX.Y.Z" only for the repository's own version.
- After this repository publishes a release: encore-basement-studio opens "Use encore-vst vX.Y.Z" when its users should get the change (the plugins are built into it, and its Windows installer bundles this release's installer), and encore-os pins the new release for its next image.

## Building and testing

- Actions in `ci.yml` are pinned to commits (version in a comment); Dependabot
  proposes updates. Keep new ones pinned the same way.
- Keep builds fast: Ninja, ccache/sccache in CI, no full LTO (`ENCORE_LTO` is
  OFF; THIN is ThinLTO/incremental LTCG), and tests link the plugins' own
  libraries (`encore_plugin_test` in `CMakeLists.txt`) instead of compiling JUCE again.
- Linux: install the packages listed in the README (JUCE 9 needs `libxi-dev`),
  then `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`, `cmake --build build`,
  `xvfb-run -a ctest --test-dir build --output-on-failure`. All must pass before a
  pull request.
- Windows: `build-and-install.ps1`; the installer: `installer/package.ps1`.
- macOS: a universal build (`"-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"`), `ctest`,
  then `installer/macos/package.sh <version>` for the .pkg and zip.

## Versioning and releases

- Semantic versioning, tags `vMAJOR.MINOR.PATCH`. MAJOR for anything that breaks
  people's saved work (a changed plugin ID or parameter, or presets that no longer
  load); MINOR for new features; PATCH for fixes.

## Release process

1. The version lives in exactly one place: `project(EncorePlugins VERSION x.y.z)`
   in `CMakeLists.txt`.
2. Changing it is its own small pull request ("Release vX.Y.Z"), so a person sees the
   number before it's built. It includes the release notes, ready to paste (see
   Release notes below).
3. Merging builds everything on `main`: the tests run, and the Windows installer and
   VST3 zip, the Mac .pkg and zip, and the Linux x64 and arm64 archives are uploaded
   as workflow artifacts.
4. After the `main` build of that commit has passed, a person publishes the release in
   GitHub. Never a Claude session. Releases, Draft a new release; tag `vX.Y.Z` (Create
   new tag) with target `main` at the "Release vX.Y.Z" commit; title `ENCORE. VST vX.Y.Z`;
   paste the release notes from the pull request; leave Previous tag as it is, don't
   click Generate release notes and don't attach files (the release job attaches
   them); Publish. Never publish an old draft release: publishing creates its tag on
   whatever its target is at that moment.
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

## Release notes

Every release's notes use this format, so they read the same in every ENCORE.
repository:

```
<One or two sentences: what this release is.>

### What's new        (a repository's first release: "What's in it")
### Downloads         (the files attached, and what each is for)
### Built on          (exactly what the Pinned releases table pinned for this release)

Questions or problems: support@encoremusic.studio
```

- The title is `ENCORE. VST vX.Y.Z`; the tag stays `vX.Y.Z`.
- The notes describe the release as it is on the day it's published: no links
  to or mentions of later releases, and no promises ("coming soon", "will
  support"). If a limitation matters to people, state the limitation, not a plan.
- They're written once, in plain language, in the "Release vX.Y.Z" pull
  request (under "Release notes (paste when publishing)"), and not edited after
  publishing except to fix a factual error.
- "Built on" names each top-level pin's release, and what it brings in
  brackets when that matters to people (for example the JUCE version).

## Changes

- main only changes through pull requests. Never push to main directly.

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
- Docs (README, CLAUDE.md) describe this repository and what it directly uses
  or is used by, never a map of ENCORE.'s other repositories.
- Docs say what the repository does and point to the file that sets a value
  (`ci.yml`, `dependabot.yml`, a CMake file, the Pinned releases table). They
  never repeat a value from config or code, and never give an estimate or a
  measurement (how long, how fast, how often). Release notes are the
  exception: each describes its release on the day it was published.
- The product: VST3 on Windows x64 (with the installer), VST3 plus Audio Units
  on macOS (universal, with a .pkg installer), and VST3 plus LV2 on Linux x64
  and arm64 (ENCORE. OS on Raspberry Pi).
- Mac builds are signed ad hoc only, not with a Developer ID, and not notarized
  (ENCORE. has no Apple Developer account). The README tells people how to open
  them (Privacy & Security, Open Anyway).
- Green Room code must never be added here (Green Room is part of the ENCORE. app, in the encoremusic repo).

## Layout

| Path | What |
|---|---|
| `plugins/<Name>/` | one folder per plugin; each folder's CMake target has the same name |
| `shared/` | `EncoreProcessorBase.h` and `ui/` (controls, presets, theme, `fonts/`) |
| `installer/` | ENCORE. VST Setup (Windows) and `package.ps1`, which builds it; `macos/package.sh`, the Mac .pkg and zip |
| `tests/` | PresetTests, SetupTests, EditorShot (the Shot<Name> targets), MasterBench (Master's CPU use, `-DENCORE_VST_BENCH=ON`) |
| `external/encore-audio-processing` | the audio processing headers, pinned to a release (the README's Pinned releases table); its `external/JUCE` is the JUCE the plugins build with |
| `CHANGELOG.md` | every release's notes, newest first; the release job copies the version's section onto the GitHub release |
| `.github/release-notes.sh` | prints a version's notes from `CHANGELOG.md`, and fails if they're missing (CI runs it on every build) |
| `.github/pins/check.sh` | CI's check that every pin is a release (the README's Pinned releases table) |
| `.github/no-audio-processing.sh` | CI's check that the plugins keep no audio processing of their own |
| `build-and-install.ps1`, `Install ENCORE Plugins.cmd` | Windows: build, test and install locally |
| `.github/workflows/ci.yml` | pull requests: Linux x64, Windows x64 and macOS (Apple Silicon), build, tests and pluginval, no packaging (skipped when only docs change, or only the version line and `CHANGELOG.md`); main and manual runs: Windows x64 with the installer, macOS universal with the .pkg, Linux x64 and arm64, as artifacts; on a schedule: strictest pluginval and sanitizers; `v*` tags: attach main's files to the Release, no rebuild; "CI passed" is the one check to require |
| `.github/sanitizers/` | the scheduled sanitizer run's exceptions (`ubsan.supp`, each with its reason; keep it short) and `report.sh` |
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
- All audio processing lives in encore-audio-processing; the plugins keep none
  of their own, not even a small helper, and don't use JUCE's processing
  classes (filters, smoothers, oversampling, FFT, levels) in its place. That
  includes what an editor draws from the sound (a spectrum, a curve, the
  reflections) and how its meters move: it reads the values from the headers.
  The factory presets are that repository's list too (`dsp/Presets.h`, made
  by its `tools/presets.py`): change a preset there. When something is
  missing there, add it there first. `.github/no-audio-processing.sh` (in CI)
  fails on processing-shaped code in `plugins/` and `shared/`.
- `StrumTests` is built from `external/encore-audio-processing/tests/StrumTests.cpp`
  with `ENCORE_STRUM_PLUGIN_TESTS=1`, and `ReferenceTests` from its
  `tests/ReferenceTests.cpp` with the plugins' compiler and flags (it checks
  every result in its `data/reference.json`). The other audio processing
  suites run in that repo, not here.

## Pins

- What this repository uses is pinned in one place, and only to a published
  release. The Pinned releases table in the README names each pin, and CI
  (`.github/pins/check.sh`) checks that each one is a release and is exactly
  what the folder is pinned to. An exception (no usable release exists) is
  written in the table with its reason.
- Fetch pinned folders by their paths, one level at a time
  (`git -C <folder> submodule update --init --depth 1 <path>`), never with
  `--recursive`.
- Move a pin to a new release only when something changes for this
  repository's users, and pick this repository's version number by what its
  users notice, not by the number of what it uses.
- Pull request titles: "Use <repository> vX.Y.Z (was vA.B.C)" for a pull
  request that moves a pin; "Release vX.Y.Z" only for this repository's own
  version.

## Building and testing

- Actions in `ci.yml` are pinned to commits (version in a comment); Dependabot
  proposes updates. Keep new ones pinned the same way.
- Keep builds fast: Ninja, ccache/sccache in CI, no full LTO (`ENCORE_LTO` is
  OFF; THIN is ThinLTO/incremental LTCG), and tests link the plugins' own
  libraries (`encore_plugin_test` in `CMakeLists.txt`) instead of compiling JUCE again.
- Linux release files are built inside Debian 12 (`container: debian:bookworm`) so
  they run on Raspberry Pi OS Bookworm, Debian 12, Ubuntu 22.04 and newer;
  `.github/linux-compat.sh` fails CI if a plugin needs newer. Don't move that job
  to a newer system without deciding to drop those.
- Windows: the Visual C++ runtime is built in (`CMAKE_MSVC_RUNTIME_LIBRARY` in
  `CMakeLists.txt`), so the plugins and Setup need nothing else installed;
  `.github/windows-runtime.ps1` fails CI if one needs `msvcp140.dll` or the like.
- Linux: install the packages listed in the README (JUCE needs `libxi-dev`),
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
   number before it's built. It adds the release notes to `CHANGELOG.md` (see
   Release notes below); CI fails until they're there.
3. Merging builds everything on `main`: the tests run, and the Windows installer and
   VST3 zip, the Mac .pkg and zip, and the Linux x64 and arm64 archives are uploaded
   as workflow artifacts.
4. After the `main` build of that commit has passed, a person publishes the release in
   GitHub. Never a Claude session. Releases, Draft a new release; tag `vX.Y.Z` (Create
   new tag) with target `main` at the "Release vX.Y.Z" commit; leave the title and the
   description empty, don't click Generate release notes and don't attach files (the
   release job fills all of them in); Publish. Never publish an old draft release:
   publishing creates its tag on whatever its target is at that moment.
5. The tag's CI run doesn't rebuild. It checks that the tag matches the version, finds
   the successful `main` build of that exact commit and attaches its files, each with a
   `.sha256` file, to the Release, and sets its title (`ENCORE. VST vX.Y.Z`) and notes (the
   version's section of `CHANGELOG.md`). If either check fails, nothing is attached.
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
- They live in `CHANGELOG.md`, newest first, one `## vX.Y.Z` section each,
  written once, in plain language, in the "Release vX.Y.Z" pull request. CI
  (`.github/release-notes.sh`) fails while the version has no section, or the
  section has no Downloads, Built on or Questions line. The release job copies
  the section onto the GitHub release, so nobody pastes notes.
- A published section isn't edited except to fix a factual error. Then fix it
  in a pull request and make the same fix on the GitHub release (Edit), so the
  two stay the same.
- "Built on" names each top-level pin's release, and what it brings in
  brackets when that matters to people (for example the JUCE version).

## Changes

- main only changes through pull requests. Never push to main directly.

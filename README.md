# Encore plugins

Encore's audio plugins (VST3, Windows) built with JUCE: Encore: Vox, Encore: Verb,
Encore: Equalize, Encore: Master and Encore: Strum. The same DSP runs in the browser as Encore
Studio's mastering engine (`web/master` in jimmyrlondo/encore-studio). Windows file names can't contain a
colon, so the files and host lists say "Encore Vox" etc.

(These were called "Sick & Sad" before. Plugin IDs didn't change, so old
Cubase projects open with the renamed plugins.)

| Plugin | What it is |
|---|---|
| **Vox** | All-in-one vocal chain: clean (low cut, gate, de-ess), tune, harmony, rider, tone, dynamics, space |
| **Verb** | Algorithmic reverb: Room, Chamber, Plate, Hall, Huge (with shimmer), Spring; ducking, movement, width |
| **Master** | One-click mastering: 8 styles, Listen learns the song's level and tone, true-peak limiter, LUFS targets, loudness-matched A/B |
| **Strum** | Acoustic guitar fixer: Body (pickup to mic: Learn from a mic on the side-chain, or a generic curve), Boom, Squeak, Pick, Double, loudness-matched A/B |
| **Equalize** | Manual 8-band parametric EQ with live spectrum, plus a Smart tab: Carve (side-chain ducking EQ) and Smart bands (dynamic cuts, with a resonance finder) |

Every plugin has factory presets plus user presets (`Documents\Encore\<Plugin>\Presets\*.encorepreset`); presets from before the rename are copied over automatically.

## Layout

- `plugins/<Name>/`: one folder per plugin
- `shared/dsp/`: DSP building blocks shared across plugins
- `tests/`: offline DSP tests
- `JUCE/`: framework (git submodule, pinned to 8.0.4)

## Build

Needs Visual Studio 2022 Build Tools with the C++ workload.

```powershell
git clone --recurse-submodules https://github.com/jimmyrlondo/sick-and-sad.git
cd sick-and-sad
.\build-and-install.ps1   # builds, runs tests, installs VST3s (asks for admin)
```

Or just double-click `Install Encore Plugins.cmd` (close Cubase first). It also removes the old "Sick & Sad" copies so Cubase doesn't list each plugin twice.

### Building on Mac/Linux

Every push is built and tested in GitHub Actions on Windows x64, macOS
(universal, Apple Silicon + Intel, macOS 11+), Linux x64 and Linux arm64 (the
same build as 64-bit Raspberry Pi OS). Each run's page has a zip of the plugins
per platform. They are not signed yet, so on a Mac you'll need to clear the
quarantine flag (`xattr -dr com.apple.quarantine <plugin>`) before a DAW loads them.

Mac (Xcode command line tools, CMake and Ninja; builds VST3 + AU):

```sh
cd sick-and-sad
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
cmake --build build
```

Linux / Raspberry Pi OS (builds VST3 + LV2):

```sh
cd sick-and-sad
sudo apt install build-essential cmake ninja-build libasound2-dev libjack-jackd2-dev \
  libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev \
  libxext-dev libxinerama-dev libxrandr-dev libxrender-dev
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The plugins land in `build/plugins/<Name>/<Target>_artefacts/Release/` (`VST3/`,
`AU/`, `LV2/`). Copy them to `~/Library/Audio/Plug-Ins/VST3` and `.../Components`
on a Mac, or `~/.vst3` and `~/.lv2` on Linux. The tests are in
`build/<Test>_artefacts/Release/`. The branded installer is Windows only.

## License

Copyright (C) 2026 Encore Studio. All rights reserved. The ENCORE. plugins are
proprietary; see `LICENSE`. The ENCORE. name and logo: see `TRADEMARKS.md`.

ENCORE. Green Room (the web tools that used to be in `web/`) moved to
jimmyrlondo/encore-studio on 2026-11-04, with its history. Its rules for the team
(contributor agreement, no copyleft code, licence tiers) are in that repo's `CLAUDE.md`.

# Encore plugins

Encore's audio plugins (VST3, Windows) built with JUCE: Encore: Vox, Encore: Verb,
Encore: Equalize and Encore: Master. The same DSP runs in the browser as Encore
Studio's mastering engine (`web/master`). Windows file names can't contain a
colon, so the files and host lists say "Encore Vox" etc.

(These were called "Sick & Sad" before. Plugin IDs didn't change, so old
Cubase projects open with the renamed plugins.)

| Plugin | What it is |
|---|---|
| **Vox** | All-in-one vocal chain: clean (low cut, gate, de-ess), tune, harmony, rider, tone, dynamics, space |
| **Verb** | Algorithmic reverb: Room, Chamber, Plate, Hall, Huge (with shimmer), Spring; ducking, movement, width |
| **Master** | One-click mastering: 8 styles, Listen learns the song's level and tone, true-peak limiter, LUFS targets, loudness-matched A/B |
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

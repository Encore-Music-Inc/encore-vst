# Sick & Sad

A collection of audio plugins (VST3, Windows) built with JUCE.

| Plugin | What it is |
|---|---|
| **Vox** | All-in-one vocal chain: clean (low cut, gate, de-ess), tune, harmony, rider, tone, dynamics, space |

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

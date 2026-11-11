# Changes

Each release's notes, newest first. When a release is published, the release
job puts its section on the GitHub release (see "Release notes" in
`CLAUDE.md`). Versions before 0.5.0 were published from a different
repository.

## v0.7.0

All of the plugins' audio processing now comes from ENCORE. Audio
Processing, the same code the rest of ENCORE. uses. Three defects are fixed,
and a few things sound or read a little differently, listed below. Your
projects and presets load as before.

### What's new
- **Vox:** the tone EQ glides when you move a knob, so there's no zipper
  noise while you turn it or automate it. The reverb send is now the
  ENCORE. reverb's Space setting, with a high cut that works at 44.1 and
  48 kHz (it did nothing there before). Depending on the voice, the reverb is
  0.7 to 4 dB different in level and 2 to 4 dB darker in the top octave.
- **Master:** the true-peak meter uses the limiter's own detector, so the
  two always agree. It no longer reads higher than the real peak.
- **Strum:** "Hear Original" matches the original to the processed sound by
  loudness as people hear it, so a brighter or darker sound doesn't skew the
  comparison.
- **Vox and Strum:** the input and output meters show the louder side of a
  stereo signal, not only the left.

### What's fixed
- **Master:** three presets selected a different style from the one their
  name suggests. They now select Heavy (Punk / Loud, Emo / Big Chorus), Warm
  (Lo-fi Demo) and Bright (Clear Vocal Mix).
- **Verb:** the picture showed no early reflections in Huge mode, though
  Huge has them. It now shows every mode's reflections, each at its level.
- **Vox:** moving a tone knob made zipper noise.

### Downloads
- **Mac** (Apple Silicon and Intel, macOS 11 or newer):
  `ENCORE-VST-0.7.0-macos.pkg` installs VST3 and Audio Units (for Logic and
  GarageBand). It isn't signed yet: the first time, macOS says it was
  "Not Opened". Click **Done**, go to **System Settings > Privacy & Security**,
  click **Open Anyway**, then open the installer again. Prefer copying files
  yourself? Use `ENCORE-VST-0.7.0-macos.zip`.
- **Windows** (10 and 11, 64-bit): `ENCORE-VST-0.7.0-windows-setup.exe`. If
  SmartScreen warns you, click **More info**, then **Run anyway**. Or
  `ENCORE-VST-0.7.0-windows-x64-vst3.zip` to copy the VST3s yourself. The
  plugins are x64: on a Windows on Arm PC, load them in your DAW's x64 version.
- **Linux and Raspberry Pi** (Raspberry Pi OS Bookworm or newer, Debian 12 or
  newer, Ubuntu 22.04 or newer): `ENCORE-VST-0.7.0-linux-x64.tar.gz` (PCs) and
  `ENCORE-VST-0.7.0-linux-arm64.tar.gz` (Raspberry Pi 4 and 5, 64-bit), with
  VST3 and LV2. Copy the `.vst3` folders to `~/.vst3` and the `.lv2` folders to
  `~/.lv2`.

Each file has a `.sha256` next to it, to check the download. After
installing, rescan plugins in your DAW.

### Built on
ENCORE. Audio Processing v2.2.0, which brings JUCE 9.0.5.

Questions or problems: support@encoremusic.studio

## v0.6.1

ENCORE. VST now runs on more Windows PCs and Linux systems: on Windows it no
longer needs Microsoft's Visual C++ runtime, and on Linux it runs on Raspberry
Pi OS Bookworm, Debian 12 and Ubuntu 22.04. The plugins sound the same as in
v0.6.0.

### What's new
- **Windows:** ENCORE. VST Setup and the plugins have everything they need
  built in. Before, on a PC without Microsoft's Visual C++ runtime, Setup didn't
  start ("MSVCP140.dll was not found") and the plugins didn't load.
- **Linux and Raspberry Pi:** the plugins run on Raspberry Pi OS Bookworm or
  newer, Debian 12 or newer and Ubuntu 22.04 or newer. Before, they needed
  Raspberry Pi OS Trixie, Debian 13 or Ubuntu 24.04.
- Every plugin is checked with pluginval on Windows, Mac, Debian 12 and Ubuntu
  22.04, and on the Mac with Apple's own Audio Unit validator, before each
  release.

### Downloads
- **Mac** (Apple Silicon and Intel, macOS 11 or newer):
  `ENCORE-VST-0.6.1-macos.pkg` installs VST3 and Audio Units (for Logic and
  GarageBand). It isn't signed yet: the first time, macOS says it was
  "Not Opened". Click **Done**, go to **System Settings > Privacy & Security**,
  click **Open Anyway**, then open the installer again. Prefer copying files
  yourself? Use `ENCORE-VST-0.6.1-macos.zip`.
- **Windows** (10 and 11, 64-bit): `ENCORE-VST-0.6.1-windows-setup.exe`. If
  SmartScreen warns you, click **More info**, then **Run anyway**. Or
  `ENCORE-VST-0.6.1-windows-x64-vst3.zip` to copy the VST3s yourself. The
  plugins are x64: on a Windows on Arm PC, load them in your DAW's x64 version.
- **Linux and Raspberry Pi** (Raspberry Pi OS Bookworm or newer, Debian 12 or
  newer, Ubuntu 22.04 or newer): `ENCORE-VST-0.6.1-linux-x64.tar.gz` (PCs) and
  `ENCORE-VST-0.6.1-linux-arm64.tar.gz` (Raspberry Pi 4 and 5, 64-bit), with
  VST3 and LV2. Copy the `.vst3` folders to `~/.vst3` and the `.lv2` folders to
  `~/.lv2`.

Each file has a `.sha256` next to it, to check the download. After
installing, rescan plugins in your DAW.

### Built on
ENCORE. Audio Processing v1.0.1, which brings JUCE 9.0.5.

Questions or problems: support@encoremusic.studio

## v0.6.0

ENCORE. VST is now on Mac: five free plugins, Vox, Verb, Equalize, Master and
Strum, for Windows, Mac and Linux. They sound the same as in v0.5.0.

### What's new
- The Mac version: VST3 and Audio Units, with an installer.
- Security and crash fixes from JUCE 9.0.5.
- Every plugin is checked with pluginval on Windows, Mac and Linux, and on the
  Mac with Apple's own Audio Unit validator, before each release.

### Downloads
- **Mac** (Apple Silicon and Intel, macOS 11 or newer):
  `ENCORE-VST-0.6.0-macos.pkg` installs VST3 and Audio Units (for Logic and
  GarageBand). It isn't signed yet: the first time, macOS says it was
  "Not Opened". Click **Done**, go to **System Settings > Privacy & Security**,
  click **Open Anyway**, then open the installer again. Prefer copying files
  yourself? Use `ENCORE-VST-0.6.0-macos.zip`.
- **Windows** (10 and 11, 64-bit, with Microsoft's Visual C++ Redistributable
  for x64 installed: if Setup says "MSVCP140.dll was not found", install it
  from Microsoft first): `ENCORE-VST-0.6.0-windows-setup.exe`. If
  SmartScreen warns you, click **More info**, then **Run anyway**. Or
  `ENCORE-VST-0.6.0-windows-x64-vst3.zip` to copy the VST3s yourself.
- **Linux and Raspberry Pi**: `ENCORE-VST-0.6.0-linux-x64.tar.gz` (PCs) and
  `ENCORE-VST-0.6.0-linux-arm64.tar.gz` (Raspberry Pi 4 and 5, 64-bit
  Raspberry Pi OS), with VST3 and LV2. They need Raspberry Pi OS Trixie,
  Debian 13 or Ubuntu 24.04 or newer: they don't load on Bookworm, Debian 12
  or Ubuntu 22.04.

Each file has a `.sha256` next to it, to check the download. After
installing, rescan plugins in your DAW.

### Built on
ENCORE. Audio Processing v1.0.1, which brings JUCE 9.0.5.

Questions or problems: support@encoremusic.studio

## v0.5.0

The first release of ENCORE. VST from this repository: five free plugins,
Vox, Verb, Equalize, Master and Strum, for Windows and Linux.

### What's new
- The plugins carry the ENCORE. name everywhere: "ENCORE. Vox" and so on in
  your DAW, presets in `Documents\ENCORE`.
- The Windows installer follows the usual Windows pattern: Uninstall removes
  everything, Modify adds or removes single plugins, and no console window
  appears.

### Downloads
- **Windows** (10 and 11, 64-bit, with Microsoft's Visual C++ Redistributable
  for x64 installed: if Setup says "MSVCP140.dll was not found", install it
  from Microsoft first): `ENCORE-VST-0.5.0-windows-setup.exe`. Tick
  the plugins you want. If SmartScreen warns you, click **More info**, then
  **Run anyway** (it isn't code-signed yet). To add or remove plugins later,
  run it again or use **Modify** on ENCORE. VST in Windows **Settings > Apps**;
  **Uninstall** there removes all of them. Your presets are never touched.
  Or `ENCORE-VST-0.5.0-windows-x64-vst3.zip` to copy the VST3s yourself.
- **Linux and Raspberry Pi**: `ENCORE-VST-0.5.0-linux-x64.tar.gz` (PCs) and
  `ENCORE-VST-0.5.0-linux-arm64.tar.gz` (Raspberry Pi 4 and 5, 64-bit
  Raspberry Pi OS), with VST3 and LV2. They need Raspberry Pi OS Trixie,
  Debian 13 or Ubuntu 24.04 or newer: they don't load on Bookworm, Debian 12
  or Ubuntu 22.04.

Each file has a `.sha256` next to it, to check the download. After
installing, rescan plugins in your DAW.

### Built on
ENCORE. Audio Processing v1.0.0, and JUCE 9 at commit `7ff644f` (between the
9.0.4 and 9.0.5 releases).

Questions or problems: support@encoremusic.studio

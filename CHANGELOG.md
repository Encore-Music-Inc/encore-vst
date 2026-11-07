# Changes

Each release's notes, newest first. When a release is published, the release
job puts its section on the GitHub release (see "Release notes" in
`CLAUDE.md`). Versions before 0.5.0 were published from a different
repository.

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
- **Windows** (10 and 11, 64-bit): `ENCORE-VST-0.6.0-windows-setup.exe`. If
  SmartScreen warns you, click **More info**, then **Run anyway**. Or
  `ENCORE-VST-0.6.0-windows-x64-vst3.zip` to copy the VST3s yourself.
- **Linux and Raspberry Pi**: `ENCORE-VST-0.6.0-linux-x64.tar.gz` (PCs) and
  `ENCORE-VST-0.6.0-linux-arm64.tar.gz` (Raspberry Pi 4 and 5, 64-bit
  Raspberry Pi OS), with VST3 and LV2.

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
- **Windows** (10 and 11, 64-bit): `ENCORE-VST-0.5.0-windows-setup.exe`. Tick
  the plugins you want. If SmartScreen warns you, click **More info**, then
  **Run anyway** (it isn't code-signed yet). To add or remove plugins later,
  run it again or use **Modify** on ENCORE. VST in Windows **Settings > Apps**;
  **Uninstall** there removes all of them. Your presets are never touched.
  Or `ENCORE-VST-0.5.0-windows-x64-vst3.zip` to copy the VST3s yourself.
- **Linux and Raspberry Pi**: `ENCORE-VST-0.5.0-linux-x64.tar.gz` (PCs) and
  `ENCORE-VST-0.5.0-linux-arm64.tar.gz` (Raspberry Pi 4 and 5, 64-bit
  Raspberry Pi OS), with VST3 and LV2.

Each file has a `.sha256` next to it, to check the download. After
installing, rescan plugins in your DAW.

### Built on
ENCORE. Audio Processing v1.0.0, and JUCE 9 at commit `7ff644f` (between the
9.0.4 and 9.0.5 releases).

Questions or problems: support@encoremusic.studio

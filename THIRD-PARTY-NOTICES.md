# Third-party notices

This repository's own code is Copyright (c) Encore Music, Inc. and licensed
under the GNU Affero General Public License v3 (see `LICENSE`). The pieces
below come from elsewhere and keep their own licences.

## ENCORE. audio processing

- What: ENCORE.'s shared audio processing headers (`dsp/...`), release v1.0.0
- Where: git submodule at `external/encore-audio-processing`
  (https://github.com/Encore-Music-Inc/encore-audio-processing)
- Licence: GNU AGPLv3, Copyright (c) Encore Music, Inc. Its own
  `external/JUCE` submodule is for its tests only and is not used here.

## JUCE

- What: the JUCE framework, version 9, at commit `7ff644f65d7daa1428c34b9738ebb5c449dc9172`
- Where: git submodule at `external/JUCE` (https://github.com/juce-framework/JUCE)
- Used by: every plugin, the tests and the Windows installer
- Licence: dual licensed under the GNU AGPLv3 and the commercial JUCE 9
  licence. This repository uses JUCE under the AGPLv3. The full terms are in
  `external/JUCE/LICENSE.md` once the submodule is checked out.

## Steinberg VST3 SDK

- What: the VST3 SDK 3.8.0, bundled with JUCE
  (`external/JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK`)
- Used by: the VST3 builds of the plugins
- Licence: MIT. VST is a registered trademark of Steinberg Media Technologies GmbH.

## LV2

- What: the LV2 headers and libraries (LV2 1.18.0, lilv, serd, sord, sratom),
  bundled with JUCE (`.../format_types/LV2_SDK`)
- Used by: the LV2 builds of the plugins on Linux
- Licence: ISC

## Other libraries bundled with JUCE

JUCE vendors further libraries that its modules compile in, depending on the
module and platform. The ones the modules used here (`juce_audio_utils`,
`juce_dsp`, `juce_gui_basics` and what they depend on) can bring in include
zlib (Zlib), libpng (libpng-2.0), the IJG JPEG library (IJG), FLAC
(BSD-3-Clause), Ogg and Vorbis (BSD-3-Clause), Opus (BSD-3-Clause), HarfBuzz
(MIT), SheenBidi (Apache-2.0), PlutoVG and LunaSVG (MIT) and CHOC (ISC). The
complete inventory, with versions, licences and locations, is JUCE's software
bill of materials: `external/JUCE/JUCE.spdx.json`.

JUCE also ships the ASIO SDK and the AAX SDK (each GPLv3 or their vendor's
licence). Neither is enabled in this build.

## Fonts

- What: DM Sans (Regular, Bold) and Space Mono (Regular, Bold)
- Where: `shared/ui/fonts/`, built into every plugin's editor and the installer
- Licence: SIL Open Font License 1.1. The licence texts are next to the fonts
  (`DMSans-OFL.txt`, `SpaceMono-OFL.txt`); where each came from is in `SOURCE.txt`.

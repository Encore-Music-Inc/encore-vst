#!/usr/bin/env bash
# ENCORE. VST for Mac: the installer (.pkg) and a zip, from a universal build.
#
#   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" "-DENCORE_PLUGIN_FORMATS=VST3;AU"
#   cmake --build build
#   installer/macos/package.sh <version> [build dir, default build]
#
# Writes to dist/:
#   ENCORE-VST-<version>-macos.pkg   the installer: each plugin is a choice (all
#                                    ticked), VST3 to /Library/Audio/Plug-Ins/VST3,
#                                    Audio Units to /Library/Audio/Plug-Ins/Components
#   ENCORE-VST-<version>-macos.zip   the same plugins, to copy by hand
#   and a .sha256 file next to each.
#
# The plugins get an ad-hoc signature (Apple Silicon needs one to load them).
# Nothing is signed with a Developer ID or notarized yet.
set -euo pipefail

version="${1:?usage: package.sh <version> [build dir]}"
build="${2:-build}"
root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$root"

# <folder under plugins/>:<CMake target>:<bundle name>:<package id>
plugins=(
  "Vox:Vox:ENCORE Vox:vox"
  "Verb:Verb:ENCORE Verb:verb"
  "EQ:EQ:ENCORE Equalize:equalize"
  "Master:Master:ENCORE Master:master"
  "Strum:Strum:ENCORE Strum:strum"
)
describe() {
  case "$1" in
    vox) echo "All-in-one vocal chain: clean, tune, harmony, rider, tone, dynamics, space." ;;
    verb) echo "Algorithmic reverb: Room, Chamber, Plate, Hall, Huge and Spring." ;;
    equalize) echo "8-band parametric EQ with live spectrum, Carve and Smart bands." ;;
    master) echo "One-click mastering with LUFS targets and a true-peak limiter." ;;
    strum) echo "Acoustic guitar fixer: Body, Boom, Squeak, Pick and Double." ;;
  esac
}

name="ENCORE-VST-$version-macos"
work="$build/macos-package"
rm -rf "$work"
mkdir -p "$work/zip/VST3" "$work/zip/AU" "$work/pkgs" dist

choices=""
outline=""
refs=""
for p in "${plugins[@]}"; do
  IFS=: read -r dir target bundle id <<< "$p"
  out="$build/plugins/$dir/${target}_artefacts/Release"
  vst3="$out/VST3/$bundle.vst3"
  au="$out/AU/$bundle.component"
  test -d "$vst3" || { echo "missing $vst3" >&2; exit 1; }
  test -d "$au" || { echo "missing $au" >&2; exit 1; }

  # Both architectures in every binary
  for b in "$vst3/Contents/MacOS/"* "$au/Contents/MacOS/"*; do
    lipo "$b" -verify_arch arm64 x86_64 || { echo "$b is not universal" >&2; lipo -info "$b" >&2; exit 1; }
  done

  # This plugin's part of the installer: its VST3 and its Audio Unit
  pr="$work/roots/$id"
  mkdir -p "$pr/Library/Audio/Plug-Ins/VST3" "$pr/Library/Audio/Plug-Ins/Components"
  ditto "$vst3" "$pr/Library/Audio/Plug-Ins/VST3/$bundle.vst3"
  ditto "$au" "$pr/Library/Audio/Plug-Ins/Components/$bundle.component"
  for b in "$pr/Library/Audio/Plug-Ins/VST3/$bundle.vst3" "$pr/Library/Audio/Plug-Ins/Components/$bundle.component"; do
    codesign --force --sign - --timestamp=none "$b"
    codesign --verify --strict "$b"
  done
  ditto "$pr/Library/Audio/Plug-Ins/VST3/$bundle.vst3" "$work/zip/VST3/$bundle.vst3"
  ditto "$pr/Library/Audio/Plug-Ins/Components/$bundle.component" "$work/zip/AU/$bundle.component"

  # Installed where they belong, even if a copy was moved elsewhere
  # (pkgbuild makes bundles relocatable unless told not to)
  plist="$work/$id.plist"
  pkgbuild --analyze --root "$pr" "$plist" > /dev/null
  python3 -I - "$plist" <<'PY'
import plistlib, sys
path = sys.argv[1]
with open(path, "rb") as f:
    bundles = plistlib.load(f)
for b in bundles:
    b["BundleIsRelocatable"] = False
with open(path, "wb") as f:
    plistlib.dump(bundles, f)
PY
  pkgbuild --root "$pr" --component-plist "$plist" --install-location / \
    --identifier "studio.encoremusic.vst.$id" --version "$version" \
    "$work/pkgs/$id.pkg" > /dev/null

  outline+="    <line choice=\"$id\"/>"$'\n'
  choices+="  <choice id=\"$id\" title=\"ENCORE. ${bundle#ENCORE }\" description=\"$(describe "$id") VST3 and Audio Unit.\">"$'\n'
  choices+="    <pkg-ref id=\"studio.encoremusic.vst.$id\"/>"$'\n'
  choices+="  </choice>"$'\n'
  refs+="  <pkg-ref id=\"studio.encoremusic.vst.$id\" version=\"$version\" onConclusion=\"none\">$id.pkg</pkg-ref>"$'\n'
done

cat > "$work/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>ENCORE. VST $version</title>
  <options customize="always" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
  <volume-check>
    <allowed-os-versions>
      <os-version min="11.0"/>
    </allowed-os-versions>
  </volume-check>
  <choices-outline>
$outline  </choices-outline>
$choices$refs</installer-gui-script>
EOF

productbuild --distribution "$work/distribution.xml" --package-path "$work/pkgs" \
  --identifier studio.encoremusic.vst --version "$version" "dist/$name.pkg"

cp LICENSE THIRD-PARTY-NOTICES.md TRADEMARKS.md "$work/zip/"
(cd "$work/zip" && ditto -c -k --sequesterRsrc . "$root/dist/$name.zip")

cd dist
shasum -a 256 "$name.pkg" > "$name.pkg.sha256"
shasum -a 256 "$name.zip" > "$name.zip.sha256"
ls -l
cat "$name.pkg.sha256" "$name.zip.sha256"

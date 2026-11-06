# Builds every ENCORE. plugin (Release), runs the plugin tests, then
# installs the VST3s to the shared folder Cubase scans (one admin prompt).
# Close Cubase first, or Windows won't let the old plugin files be replaced.
$ErrorActionPreference = "Stop"
# CMake from the VS 2022 Build Tools if they're installed, otherwise the one on PATH
$cmake = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if (-not (Test-Path $cmake)) { $cmake = (Get-Command cmake -ErrorAction Stop).Source }

Set-Location $PSScriptRoot

# The submodules (JUCE and the audio processing code). Not --recursive:
# encore-audio-processing's own external/JUCE is only for its own tests.
if (-not (Test-Path "external\JUCE\CMakeLists.txt") -or -not (Test-Path "external\encore-audio-processing\dsp\MasterChain.h")) {
    git submodule update --init external/JUCE external/encore-audio-processing
    if ($LASTEXITCODE -ne 0) { throw "git submodule update failed" }
}
if (-not (Test-Path build)) { & $cmake -B build -G "Visual Studio 17 2022" -A x64 }
& $cmake --build build --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

foreach ($t in "PresetTests", "StrumTests") {
    Write-Host "`n== $t" -ForegroundColor Cyan
    & "$PSScriptRoot\build\$($t)_artefacts\Release\$t.exe"
    if ($LASTEXITCODE -ne 0) { throw "$t failed, not installing" }
}

$plugins = @(
    @{ Dir = "Vox";  Target = "Vox";   Name = "Encore Vox";      Old = "Sick & Sad Vox"  },
    @{ Dir = "Verb"; Target = "Verb";  Name = "Encore Verb";     Old = "Sick & Sad Verb" },
    @{ Dir = "EQ";   Target = "SnsEQ"; Name = "Encore Equalize"; Old = "Sick & Sad EQ"   },
    @{ Dir = "Master"; Target = "Master"; Name = "Encore Master"; Old = "Sick & Sad Master" },
    @{ Dir = "Strum"; Target = "Strum"; Name = "Encore Strum"; Old = "Sick & Sad Strum" }
)
$vst = "$env:CommonProgramFiles\VST3"
# The plugins used to be called "Sick & Sad ..." with the same IDs; remove those
# copies so Cubase doesn't find each plugin twice (projects keep working: the IDs match).
$copies = foreach ($p in $plugins) {
    "if exist `"$vst\$($p.Old).vst3`" rmdir /s /q `"$vst\$($p.Old).vst3`""
    $src = "$PSScriptRoot\build\plugins\$($p.Dir)\$($p.Target)_artefacts\Release\VST3\$($p.Name).vst3"
    "robocopy `"$src`" `"$vst\$($p.Name).vst3`" /E /NFL /NDL /NJH /NJS"
}
Start-Process cmd -ArgumentList "/c", ($copies -join " & ") -Verb RunAs -Wait

Write-Host ""
foreach ($p in $plugins) {
    $src = "$PSScriptRoot\build\plugins\$($p.Dir)\$($p.Target)_artefacts\Release\VST3\$($p.Name).vst3\Contents\x86_64-win\$($p.Name).vst3"
    $dst = "$vst\$($p.Name).vst3\Contents\x86_64-win\$($p.Name).vst3"
    $ok = (Test-Path -LiteralPath $dst) -and ((Get-FileHash -LiteralPath $src).Hash -eq (Get-FileHash -LiteralPath $dst).Hash)
    if ($ok) { Write-Host "Installed $($p.Name)" -ForegroundColor Green }
    else { Write-Host "NOT installed: $($p.Name) (is Cubase still open?)" -ForegroundColor Red }
}

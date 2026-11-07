# Builds every ENCORE. plugin (Release), runs the plugin tests, then
# installs the VST3s to the shared folder Cubase scans (one admin prompt).
# Close Cubase first, or Windows won't let the old plugin files be replaced.
$ErrorActionPreference = "Stop"
# CMake from the VS 2022 Build Tools if they're installed, otherwise the one on PATH
$cmake = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if (-not (Test-Path $cmake)) { $cmake = (Get-Command cmake -ErrorAction Stop).Source }

Set-Location $PSScriptRoot

# The audio processing code and the JUCE it pins (the JUCE version for all of
# ENCORE.), fetched by path, not --recursive
if (-not (Test-Path "external\encore-audio-processing\dsp\MasterChain.h") -or -not (Test-Path "external\encore-audio-processing\external\JUCE\CMakeLists.txt")) {
    git submodule update --init --depth 1 external/encore-audio-processing
    if ($LASTEXITCODE -ne 0) { throw "git submodule update failed" }
    git -C external/encore-audio-processing submodule update --init --depth 1 external/JUCE
    if ($LASTEXITCODE -ne 0) { throw "git submodule update failed (JUCE)" }
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
    @{ Dir = "Vox";    Target = "Vox";    Name = "ENCORE Vox" },
    @{ Dir = "Verb";   Target = "Verb";   Name = "ENCORE Verb" },
    @{ Dir = "EQ";     Target = "EQ";     Name = "ENCORE Equalize" },
    @{ Dir = "Master"; Target = "Master"; Name = "ENCORE Master" },
    @{ Dir = "Strum";  Target = "Strum";  Name = "ENCORE Strum" }
)
$vst = "$env:CommonProgramFiles\VST3"
$copies = foreach ($p in $plugins) {
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

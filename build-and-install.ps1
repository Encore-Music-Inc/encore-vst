# Builds the Sick & Sad plugins (Release), runs the DSP tests, and copies the
# VST3s to the shared folder Cubase scans.
# Close Cubase first, or Windows won't let the old plugin file be replaced.
$ErrorActionPreference = "Stop"
$cmake = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

Set-Location $PSScriptRoot
if (-not (Test-Path build)) { & $cmake -B build -G "Visual Studio 17 2022" -A x64 }
& $cmake --build build --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

& "$PSScriptRoot\build\PitchTests_artefacts\Release\PitchTests.exe"
if ($LASTEXITCODE -ne 0) { throw "DSP tests failed, not installing" }

$src = "$PSScriptRoot\build\plugins\Vox\Vox_artefacts\Release\VST3\Sick & Sad Vox.vst3"
$dst = "$env:CommonProgramFiles\VST3\Sick & Sad Vox.vst3"
Start-Process robocopy -ArgumentList "`"$src`" `"$dst`" /E /NFL /NDL /NJH /NJS" -Verb RunAs -Wait
Write-Host "Installed to $dst"

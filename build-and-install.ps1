# Builds every Sick & Sad plugin (Release), runs all the DSP tests, then
# installs the VST3s to the shared folder Cubase scans (one admin prompt).
# Close Cubase first, or Windows won't let the old plugin files be replaced.
$ErrorActionPreference = "Stop"
$cmake = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

Set-Location $PSScriptRoot
if (-not (Test-Path build)) { & $cmake -B build -G "Visual Studio 17 2022" -A x64 }
& $cmake --build build --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

foreach ($t in "PitchTests", "PresetTests", "VerbTests", "EqTests", "MasterTests") {
    Write-Host "`n== $t" -ForegroundColor Cyan
    & "$PSScriptRoot\build\$($t)_artefacts\Release\$t.exe"
    if ($LASTEXITCODE -ne 0) { throw "$t failed, not installing" }
}

$plugins = @(
    @{ Dir = "Vox";  Target = "Vox";   Name = "Sick & Sad Vox"  },
    @{ Dir = "Verb"; Target = "Verb";  Name = "Sick & Sad Verb" },
    @{ Dir = "EQ";   Target = "SnsEQ"; Name = "Sick & Sad EQ"   },
    @{ Dir = "Master"; Target = "Master"; Name = "Sick & Sad Master" }
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

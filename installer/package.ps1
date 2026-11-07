# Builds ENCORE. VST Setup: every plugin (Release), all their tests, a screenshot
# of each for its card, the plugins zipped into the payload, then the installer
# and its own tests. Result: dist\ENCORE-VST-<version>-windows-setup.exe
#
#   cd <your clone of encore-vst>
#   powershell -ExecutionPolicy Bypass -File .\installer\package.ps1
#
# If build\ is already configured (CI configures it with Ninja and a compiler
# cache), that configuration is used as it is.
param([string]$Generator = "Visual Studio 17 2022")
$ErrorActionPreference = "Stop"
# CMake from the VS 2022 Build Tools if they're installed, otherwise the one on PATH
$cmake = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if (-not (Test-Path $cmake)) { $cmake = (Get-Command cmake -ErrorAction Stop).Source }
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root

# The audio processing code and the JUCE it pins (the JUCE version for all of
# ENCORE.), fetched by path, not --recursive
if (-not (Test-Path "external\encore-audio-processing\dsp\MasterChain.h") -or -not (Test-Path "external\encore-audio-processing\external\JUCE\CMakeLists.txt")) {
    git submodule update --init --depth 1 external/encore-audio-processing
    if ($LASTEXITCODE -ne 0) { throw "git submodule update failed" }
    git -C external/encore-audio-processing submodule update --init --depth 1 external/JUCE
    if ($LASTEXITCODE -ne 0) { throw "git submodule update failed (JUCE)" }
}
function Run($what) { & $what[0] $what[1..($what.Count - 1)]; if ($LASTEXITCODE -ne 0) { throw "Failed: $what" } }

if (-not (Test-Path "build\CMakeCache.txt")) {
    if ($Generator -like "Visual Studio*") { Run @($cmake, "-B", "build", "-G", $Generator, "-A", "x64") }
    else { Run @($cmake, "-B", "build", "-G", $Generator, "-DCMAKE_BUILD_TYPE=Release") }
}
Run @($cmake, "--build", "build", "--config", "Release", "--parallel")
foreach ($t in "PresetTests", "StrumTests") {
    Write-Host "== $t" -ForegroundColor Cyan
    Run @("$root\build\$($t)_artefacts\Release\$t.exe")
}

$payload = "$root\build\installer-payload"
Remove-Item -Recurse -Force $payload -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force "$payload\stage", "$payload\shots" | Out-Null

$plugins = @(
    @{ Dir = "Vox";    Target = "Vox";    Name = "ENCORE Vox";      Shot = "VOX"    },
    @{ Dir = "Verb";   Target = "Verb";   Name = "ENCORE Verb";     Shot = "VERB"   },
    @{ Dir = "EQ";     Target = "EQ";     Name = "ENCORE Equalize"; Shot = "EQ"     },
    @{ Dir = "Master"; Target = "Master"; Name = "ENCORE Master";   Shot = "MASTER" },
    @{ Dir = "Strum";  Target = "Strum";  Name = "ENCORE Strum";    Shot = "STRUM"  }
)
foreach ($p in $plugins) {
    Copy-Item -Recurse "$root\build\plugins\$($p.Dir)\$($p.Target)_artefacts\Release\VST3\$($p.Name).vst3" "$payload\stage\"
    Run @("$root\build\Shot$($p.Dir)_artefacts\Release\Shot$($p.Dir).exe", "$payload\shots\$($p.Shot).png")
}
Push-Location "$payload\stage"
Run (@($cmake, "-E", "tar", "cf", "..\plugins.zip", "--format=zip") + ($plugins | ForEach-Object { "$($_.Name).vst3" }))
Pop-Location

# configure again: the installer targets appear once the payload exists
Run @($cmake, "-S", ".", "-B", "build")
Run @($cmake, "--build", "build", "--config", "Release", "--target", "SetupTests", "EncoreVstSetup", "EncoreVstSetupDev")
Write-Host "== SetupTests" -ForegroundColor Cyan
Run @("$root\build\installer\SetupTests_artefacts\Release\SetupTests.exe")

$version = (Select-String -Path "$root\CMakeLists.txt" -Pattern 'project\(EncorePlugins VERSION ([0-9.]+)\)').Matches[0].Groups[1].Value
New-Item -ItemType Directory -Force "$root\dist" | Out-Null
$out = "$root\dist\ENCORE-VST-$version-windows-setup.exe"
Copy-Item "$root\build\installer\EncoreVstSetup_artefacts\Release\ENCORE VST Setup.exe" $out -Force
Write-Host "`nBuilt $out ($([math]::Round((Get-Item $out).Length / 1MB, 1)) MB)" -ForegroundColor Green

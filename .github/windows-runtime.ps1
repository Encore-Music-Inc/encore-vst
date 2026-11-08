# Checks that the Windows files people download run on a PC with nothing extra
# installed: no plugin or installer may need the Microsoft Visual C++ runtime
# (msvcp140.dll, vcruntime140*.dll, the api-ms-win-crt-* "Universal CRT" DLLs or
# ucrtbase.dll). CMakeLists.txt builds it in (CMAKE_MSVC_RUNTIME_LIBRARY); this
# catches a change that undoes that. GitHub's Windows machines have the runtime
# installed, so the tests alone would never notice.
#
# Usage (in the MSVC environment, for dumpbin): .github/windows-runtime.ps1 <file or folder>...
param([Parameter(Mandatory, ValueFromRemainingArguments)] [string[]] $Paths)
$ErrorActionPreference = 'Stop'
$files = foreach ($p in $Paths) {
    if (Test-Path $p -PathType Container) {
        Get-ChildItem $p -Recurse -File | Where-Object { $_.Extension -in '.exe', '.dll', '.vst3' }
    } elseif (Test-Path $p) { Get-Item $p }
}
if (-not $files) { throw "No Windows binaries under $Paths" }
$bad = $false
foreach ($f in $files) {
    $deps = (& dumpbin /nologo /dependents $f.FullName) |
        ForEach-Object { $_.Trim() } | Where-Object { $_ -match '\.dll$' }
    if ($LASTEXITCODE -ne 0) { throw "dumpbin failed on $($f.FullName)" }
    $runtime = $deps | Where-Object { $_ -match '^(msvcp\d+|vcruntime\d+(_\d+)?|concrt\d+|vcomp\d+|ucrtbase|api-ms-win-crt-.*)\.dll$' }
    if ($runtime) {
        Write-Host "::error file=$($f.FullName)::needs the Visual C++ runtime: $($runtime -join ', ')"
        $bad = $true
    } else {
        Write-Host "$($f.Name): no Visual C++ runtime needed ($($deps -join ', '))"
    }
}
if ($bad) { exit 1 }

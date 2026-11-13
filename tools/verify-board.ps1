# verify-board.ps1 -- the one gate an agent must pass before saying "done".
#
# Chain: build -> flash -> serial evidence -> size budget -> evidence bundle.
# Toolchain: ARM GCC only (build.bat / build.ps1 / Makefile).
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools\verify-board.ps1
#   powershell -ExecutionPolicy Bypass -File tools\verify-board.ps1 -Port COM3 -TimeoutSec 20
#   powershell -ExecutionPolicy Bypass -File tools\verify-board.ps1 -UpdateBaseline
#   powershell -ExecutionPolicy Bypass -File tools\verify-board.ps1 -SkipFlash
#
# Exit code: 0 = PASS, 1 = FAIL.

[CmdletBinding()]
param(
    [string]$Port,
    [int]$TimeoutSec = 15,

    [string[]]$Expect = @("OneNET Mini bare-metal project", "Hello from main()"),
    [string[]]$ExpectRegex = @("counter\s*=\s*1\b", "counter\s*=\s*2\b"),

    [switch]$SkipBuild,
    [switch]$SkipFlash,
    [switch]$NoReset,
    [switch]$Clean,
    [switch]$Release,
    [switch]$Incremental,

    [switch]$UpdateBaseline,
    [double]$MaxFlashGrowthPct = 5,
    [double]$MaxRamGrowthPct = 5,

    [switch]$NoEvidence,
    [string]$LogDir
)

. (Join-Path $PSScriptRoot "common.ps1")

$Root = Split-Path -Parent $PSScriptRoot
if (-not $LogDir) { $LogDir = Join-Path $Root "logs" }
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

$buildJson  = Join-Path $LogDir "build.json"
$flashJson  = Join-Path $LogDir "flash.json"
$serialJson = Join-Path $LogDir "serial.json"

$gates = [ordered]@{
    build  = "SKIP"
    flash  = "SKIP"
    serial = "SKIP"
    size   = "SKIP"
}
$notes = @()

function Invoke-Child([string]$Script, [object[]]$Arguments) {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $Script @Arguments | Out-Host
    return $LASTEXITCODE
}

$overall = Get-Date

# ---- 1. build --------------------------------------------------------------
if (-not $SkipBuild) {
    $childArgs = @()
    if ($Clean)       { $childArgs += "-Clean" }
    if ($Release)     { $childArgs += "-Release" }
    if ($Incremental) { $childArgs += "-Incremental" }

    $rc = Invoke-Child (Join-Path $PSScriptRoot "verify-build.ps1") $childArgs
    $gates.build = if ($rc -eq 0) { "PASS" } else { "FAIL" }
} else {
    $notes += "build skipped by request"
}

$build = Read-JsonFile $buildJson
if (-not $build) {
    Write-Err "build result missing: $buildJson"
    $gates.build = "FAIL"
}
elseif (-not $build.ok) {
    $gates.build = "FAIL"
}

if ($gates.build -eq "FAIL") {
    Write-Host ""
    Write-Err "stopping at build gate"
    Write-Host "RESULT: FAIL stage=build"
    exit 1
}

# ---- 2. flash --------------------------------------------------------------
if (-not $SkipFlash) {
    $hex = Join-Path $Root "output\gcc\OneNet_Mini_Bare.hex"
    $childArgs = @("-TimeoutSec", "120", "-Hex", $hex)
    $rc = Invoke-Child (Join-Path $PSScriptRoot "verify-flash.ps1") $childArgs
    $gates.flash = if ($rc -eq 0) { "PASS" } else { "FAIL" }
} else {
    $notes += "flash skipped by request"
}

$flash = Read-JsonFile $flashJson
if (-not $SkipFlash -and (-not $flash -or -not $flash.ok)) {
    $gates.flash = "FAIL"
}

if ($gates.flash -eq "FAIL") {
    Write-Host ""
    Write-Err "stopping at flash gate"
    Write-Host "RESULT: FAIL stage=flash"
    exit 1
}

# ---- 3. serial evidence ----------------------------------------------------
$expectJson      = ConvertTo-Json -InputObject ([object[]]@($Expect)) -Compress
$expectRegexJson = ConvertTo-Json -InputObject ([object[]]@($ExpectRegex)) -Compress

# Pass arrays through the environment: native argument quoting mangles JSON.
$env:VERIFY_EXPECT_JSON       = $expectJson
$env:VERIFY_EXPECT_REGEX_JSON = $expectRegexJson

$childArgs = @("-TimeoutSec", [string]$TimeoutSec)
if ($Port) { $childArgs += @("-Port", $Port) }
if (-not $NoReset) { $childArgs += "-Reset" }

$rc = Invoke-Child (Join-Path $PSScriptRoot "serial-capture.ps1") $childArgs

Remove-Item Env:\VERIFY_EXPECT_JSON -ErrorAction SilentlyContinue
Remove-Item Env:\VERIFY_EXPECT_REGEX_JSON -ErrorAction SilentlyContinue
$gates.serial = if ($rc -eq 0) { "PASS" } else { "FAIL" }

$serial = Read-JsonFile $serialJson
if (-not $serial -or -not $serial.ok) {
    $gates.serial = "FAIL"
}

# ---- 4. size budget --------------------------------------------------------
$baselinePath = Join-Path $PSScriptRoot "size-baseline.json"
$baseline = Read-JsonFile $baselinePath
$key = if ($Release) { "gcc-release" } else { "gcc" }

$flashGrowth = $null
$ramGrowth = $null
$sizeGateOk = $true

if ($build -and $build.size) {
    $curFlash = [int64]$build.size.flash
    $curRam   = [int64]$build.size.ram

    $hasKey = $false
    if ($baseline) {
        $hasKey = @($baseline.PSObject.Properties.Name) -contains $key
    }

    if ($UpdateBaseline -or -not $hasKey) {
        $merged = [ordered]@{}
        if ($baseline) {
            foreach ($p in $baseline.PSObject.Properties) { $merged[$p.Name] = $p.Value }
        }
        $merged[$key] = [ordered]@{
            flash   = $curFlash
            ram     = $curRam
            updated = (Get-Date).ToString("s")
            note    = "created by verify-board.ps1"
        }
        Write-JsonFile $baselinePath $merged
        $gates.size = "BASELINE"
        $notes += "size baseline written for '$key'"
    } else {
        $baseFlash = [int64]$baseline.$key.flash
        $baseRam   = [int64]$baseline.$key.ram

        if ($baseFlash -gt 0) {
            $flashGrowth = [math]::Round((($curFlash - $baseFlash) * 100.0 / $baseFlash), 2)
            if ($flashGrowth -gt $MaxFlashGrowthPct) { $sizeGateOk = $false }
        }
        if ($baseRam -gt 0) {
            $ramGrowth = [math]::Round((($curRam - $baseRam) * 100.0 / $baseRam), 2)
            if ($ramGrowth -gt $MaxRamGrowthPct) { $sizeGateOk = $false }
        }

        $gates.size = if ($sizeGateOk) { "PASS" } else { "FAIL" }
    }
} else {
    $notes += "size not parsed"
}

# ---- 5. evidence bundle ----------------------------------------------------
$evidenceDir = $null
if (-not $NoEvidence) {
    $stamp = $overall.ToString("yyyyMMdd-HHmmss")
    $evidenceDir = Join-Path $Root "docs\EVIDENCE\$stamp"
    New-Item -ItemType Directory -Force -Path $evidenceDir | Out-Null

    foreach ($f in @($buildJson, $flashJson, $serialJson,
                     (Join-Path $LogDir "size.txt"),
                     (Join-Path $LogDir "build-gcc.log"),
                     (Join-Path $LogDir "flash.log"),
                     (Join-Path $LogDir "serial.log"))) {
        if (Test-Path $f) {
            Copy-Item -Path $f -Destination $evidenceDir -Force -ErrorAction SilentlyContinue
        }
    }
    if (Test-Path (Join-Path $PSScriptRoot "size-baseline.json")) {
        Copy-Item -Path (Join-Path $PSScriptRoot "size-baseline.json") `
                  -Destination $evidenceDir -Force -ErrorAction SilentlyContinue
    }

    $commit = Get-GitCommit $Root
    $missing = if ($serial -and $serial.missing) { @($serial.missing) -join ", " } else { "" }
    $flashGrowthText = if ($null -ne $flashGrowth) { "$flashGrowth %" } else { "n/a (baseline)" }
    $ramGrowthText   = if ($null -ne $ramGrowth) { "$ramGrowth %" } else { "n/a (baseline)" }
    $summary = @"
# Board verification evidence

- time: $($overall.ToString("s"))
- commit: $commit
- toolchain: arm-none-eabi-gcc
- build gate: $($gates.build)
- flash gate: $($gates.flash)
- serial gate: $($gates.serial)
- size gate: $($gates.size)
- flash growth: $flashGrowthText
- ram growth: $ramGrowthText

## Serial expectations

- expected: $($Expect -join " | ")
- regex: $($ExpectRegex -join " | ")
- missing: $missing

## Files

- build.json, flash.json, serial.json, size.txt
- build-gcc.log, flash.log, serial.log
- size-baseline.json

Generated by tools/verify-board.ps1. Do not hand-edit this file.
"@
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText((Join-Path $evidenceDir "summary.md"), $summary, $utf8)
}

# ---- 6. verdict ------------------------------------------------------------
$allOk = $true
foreach ($k in $gates.Keys) {
    if ($gates[$k] -eq "FAIL") { $allOk = $false }
}
if (-not $SkipBuild -and $gates.build -ne "PASS") { $allOk = $false }
if (-not $SkipFlash -and $gates.flash -ne "PASS") { $allOk = $false }
if ($gates.serial -ne "PASS") { $allOk = $false }

Write-Host ""
Write-Host "================ GATE SUMMARY ================" -ForegroundColor Cyan
foreach ($k in $gates.Keys) {
    $color = "Gray"
    if ($gates[$k] -eq "PASS") { $color = "Green" }
    elseif ($gates[$k] -eq "FAIL") { $color = "Red" }
    Write-Host ("{0,-8} {1}" -f $k, $gates[$k]) -ForegroundColor $color
}
if ($flashGrowth -ne $null) { Write-Host ("flash growth: {0} %" -f $flashGrowth) }
if ($ramGrowth -ne $null)   { Write-Host ("ram growth  : {0} %" -f $ramGrowth) }
if ($notes.Count -gt 0) {
    Write-Host ""
    $notes | ForEach-Object { Write-Warn $_ }
}
if ($evidenceDir) { Write-Host "Evidence: $evidenceDir" }

Write-Host ""
if ($allOk) {
    Write-Ok "board verification passed"
    Write-Host "RESULT: PASS"
    exit 0
}

Write-Err "board verification failed"
Write-Host "RESULT: FAIL"
exit 1

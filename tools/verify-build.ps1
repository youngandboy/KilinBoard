# verify-build.ps1 -- ARM GCC build gate.
# Wraps build.bat / build.ps1 / Makefile and produces logs/build.json.
# Does not flash or touch the board.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools\verify-build.ps1
#   powershell -ExecutionPolicy Bypass -File tools\verify-build.ps1 -Clean
#   powershell -ExecutionPolicy Bypass -File tools\verify-build.ps1 -Release
#   powershell -ExecutionPolicy Bypass -File tools\verify-build.ps1 -Incremental

[CmdletBinding()]
param(
    [switch]$Clean,
    [switch]$Release,
    [switch]$Incremental,

    [string]$LogDir
)

. (Join-Path $PSScriptRoot "common.ps1")

$Root = Split-Path -Parent $PSScriptRoot
if (-not $LogDir) { $LogDir = Join-Path $Root "logs" }
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

$logPath  = Join-Path $LogDir "build-gcc.log"
$jsonPath = Join-Path $LogDir "build.json"
$sizePath = Join-Path $LogDir "size.txt"

$start = Get-Date
Write-Step "Build (arm-none-eabi-gcc)"

$cmdLine = "build.bat"
# A gate must reflect the current source: Make does not track flag changes.
if ((-not $Incremental) -or $Clean) { $cmdLine += " -Clean" }
if ($Release) { $cmdLine += " -Release" }

Push-Location $Root
try {
    $redirect = "$cmdLine > `"$logPath`" 2>&1"
    & cmd.exe /c $redirect
    $rc = $LASTEXITCODE
} finally {
    Pop-Location
}

$elapsed = ((Get-Date) - $start).TotalSeconds
$text = ""
if (Test-Path $logPath) {
    $text = Get-Content -Path $logPath -Raw -ErrorAction SilentlyContinue
}
if (-not $text) { $text = "" }

# ---- parse diagnostics -----------------------------------------------------
$errorLines = @()
$warningLines = @()
foreach ($line in ($text -split "`r?`n")) {
    $trim = $line.Trim()
    if (-not $trim) { continue }

    $isZeroSummary = $trim -match '(?i)\b0\s+(errors?|warnings?)\b'
    $isErrorLevel  = $trim -match '(?i)errorlevel'

    if (-not $isZeroSummary -and -not $isErrorLevel -and
        $trim -match '(?i)\b(fatal error|error|undefined reference|multiple definition)\b') {
        $errorLines += $trim
    }

    if (-not $isZeroSummary -and $trim -match '(?i)\bwarning\b') {
        $warningLines += $trim
    }
}

# ---- determine pass/fail ---------------------------------------------------
$ok = ($rc -eq 0)
if ($ok -and $errorLines.Count -gt 0) {
    $ok = $false
}

# ---- size ------------------------------------------------------------------
$size = $null
if ($ok) {
    $elf = Join-Path $Root "output\gcc\OneNet_Mini_Bare.elf"
    $size = Get-ElfSize $elf
}

if ($size) {
    $sizeText = @(
        "tool=gcc",
        "flash=$($size.flash) bytes",
        "ram=$($size.ram) bytes",
        "text=$($size.text) data=$($size.data) bss=$($size.bss)",
        "raw=$($size.raw)"
    ) -join "`r`n"
    Set-Content -Path $sizePath -Value $sizeText -Encoding ASCII
}

# ---- result ----------------------------------------------------------------
$result = [ordered]@{
    tool         = "gcc"
    ok           = $ok
    exitCode     = $rc
    started      = $start.ToString("s")
    elapsedSec   = [math]::Round($elapsed, 2)
    log          = $logPath
    sizeLog      = $sizePath
    errors       = @($errorLines | Select-Object -First 20)
    warningCount = $warningLines.Count
    size         = $size
}
Write-JsonFile $jsonPath $result

Write-Host ""
Write-Host "---- build output (tail) ----" -ForegroundColor DarkGray
Get-LogTail $logPath 40 | ForEach-Object { Write-Host $_ }

Write-Host ""
if ($size) {
    Write-Host ("Size : flash={0} B, ram={1} B" -f $size.flash, $size.ram)
}
Write-Host ("Exit : {0}   Warnings: {1}" -f $rc, $warningLines.Count)
Write-Host ("Log  : {0}" -f $logPath)

if ($ok) {
    Write-Ok "build passed"
    Write-Host "RESULT: BUILD_OK tool=gcc"
    exit 0
}

Write-Err "build failed"
if ($errorLines.Count -gt 0) {
    Write-Host ""
    Write-Host "First errors:" -ForegroundColor Red
    $errorLines | Select-Object -First 10 | ForEach-Object { Write-Host "  $_" }
}
Write-Host "RESULT: BUILD_FAIL tool=gcc exit=$rc"
exit 1

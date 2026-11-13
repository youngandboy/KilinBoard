# verify-flash.ps1 -- flash gate.
# Wraps flash.bat, captures ST-LINK_CLI output, and writes logs/flash.json.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools\verify-flash.ps1
#   powershell -ExecutionPolicy Bypass -File tools\verify-flash.ps1 -Full
#   powershell -ExecutionPolicy Bypass -File tools\verify-flash.ps1 -Hex output\gcc\OneNet_Mini_Bare.hex

[CmdletBinding()]
param(
    [string]$Hex,
    [switch]$Full,
    [int]$TimeoutSec = 120,
    [string]$LogDir
)

. (Join-Path $PSScriptRoot "common.ps1")

$Root = Split-Path -Parent $PSScriptRoot
if (-not $LogDir) { $LogDir = Join-Path $Root "logs" }
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

if (-not $Hex) {
    $Hex = Join-Path $Root "output\gcc\OneNet_Mini_Bare.hex"
}

if (-not $Hex -or -not (Test-Path $Hex)) {
    $msg = "HEX not found. Run verify-build.ps1 first."
    Write-Err $msg
    Write-JsonFile (Join-Path $LogDir "flash.json") ([ordered]@{
        ok = $false; exitCode = -1; hex = $Hex; error = $msg
    })
    Write-Host "RESULT: FLASH_FAIL reason=no_hex"
    exit 1
}

$cli = Get-StLinkCli
if (-not $cli) {
    $msg = "ST-LINK_CLI.exe not found. Set STLINK_CLI to its full path."
    Write-Err $msg
    Write-JsonFile (Join-Path $LogDir "flash.json") ([ordered]@{
        ok = $false; exitCode = -1; hex = $Hex; error = $msg
    })
    Write-Host "RESULT: FLASH_FAIL reason=no_stlink_cli"
    exit 1
}

$logPath  = Join-Path $LogDir "flash.log"
$jsonPath = Join-Path $LogDir "flash.json"

$cmdLine = "flash.bat"
if ($Full) { $cmdLine += " -Full" }
$cmdLine += " `"$Hex`""

Write-Step "Flash"
Write-Host "HEX : $Hex"
Write-Host "CLI : $cli"
Write-Host "Mode: $(if ($Full) { 'SWD + full chip erase' } else { 'SWD' })"

$redirect = "$cmdLine > `"$logPath`" 2>&1"
$start = Get-Date

Push-Location $Root
try {
    $proc = Start-Process -FilePath "cmd.exe" -ArgumentList @("/c", $redirect) `
                          -NoNewWindow -PassThru
    # Opening the handle makes ExitCode available after WaitForExit(timeout) on PS 5.1.
    $null = $proc.Handle
    $finished = $proc.WaitForExit($TimeoutSec * 1000)
    if (-not $finished) {
        try { $proc.Kill() } catch { }
        $rc = -2
    } else {
        $rc = $proc.ExitCode
    }
} finally {
    Pop-Location
}

$elapsed = ((Get-Date) - $start).TotalSeconds
$text = ""
if (Test-Path $logPath) {
    $text = Get-Content -Path $logPath -Raw -ErrorAction SilentlyContinue
}
if (-not $text) { $text = "" }

$alreadyUsed = $text -match '(?i)ST-LINK\s+Already\s+Used'
$verifyOk    = $text -match '(?i)Verification\.\.\.OK'
$started     = $text -match '(?i)Application started'
$flashOk     = $text -match '(?i)Flash OK'

$ok = ($rc -eq 0) -and $flashOk -and $verifyOk -and -not $alreadyUsed
if ($rc -eq -2) {
    $ok = $false
    Write-Err "flash timed out after $TimeoutSec s"
}

$result = [ordered]@{
    tool       = "stlink"
    ok         = $ok
    exitCode   = $rc
    hex        = $Hex
    fullErase  = [bool]$Full
    elapsedSec = [math]::Round($elapsed, 2)
    log        = $logPath
    signals    = [ordered]@{
        flashOk      = [bool]$flashOk
        verifyOk     = [bool]$verifyOk
        appStarted   = [bool]$started
        probeInUse   = [bool]$alreadyUsed
    }
}
Write-JsonFile $jsonPath $result

Write-Host ""
Write-Host "---- flash output (tail) ----" -ForegroundColor DarkGray
Get-LogTail $logPath 30 | ForEach-Object { Write-Host $_ }

Write-Host ""
Write-Host ("Exit : {0}   Verify: {1}   Started: {2}" -f $rc, $verifyOk, $started)
Write-Host ("Log  : {0}" -f $logPath)

if ($ok) {
    Write-Ok "flash passed"
    Write-Host "RESULT: FLASH_OK"
    exit 0
}

Write-Err "flash failed"
if ($alreadyUsed) {
    Write-Host "Close STM32 ST-LINK Utility (GUI) and retry: it holds the probe exclusively." -ForegroundColor Yellow
}
if ($rc -eq -2) {
    Write-Host "RESULT: FLASH_FAIL reason=timeout"
} else {
    Write-Host "RESULT: FLASH_FAIL exit=$rc"
}
exit 1


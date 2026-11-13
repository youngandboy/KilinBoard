# serial-capture.ps1 -- serial evidence gate.
# Opens the debug UART, optionally resets the target through ST-LINK so the
# boot banner is captured, and asserts expected strings/regexes.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools\serial-capture.ps1 -ListPorts
#   powershell -ExecutionPolicy Bypass -File tools\serial-capture.ps1 -Port COM3
#   powershell -ExecutionPolicy Bypass -File tools\serial-capture.ps1 -Reset -TimeoutSec 12
#   powershell -ExecutionPolicy Bypass -File tools\serial-capture.ps1 `
#       -Expect "OneNET Mini bare-metal project" -ExpectRegex "counter\s*=\s*\d+"

[CmdletBinding()]
param(
    [string]$Port,
    [int]$Baud = 115200,
    [int]$TimeoutSec = 15,
    [string[]]$Expect = @("OneNET Mini bare-metal project", "Hello from main()"),
    [string[]]$ExpectRegex = @("counter\s*=\s*1\b", "counter\s*=\s*2\b"),
    [string]$ExpectJson,
    [string]$ExpectRegexJson,
    [switch]$Reset,
    [switch]$ListPorts,
    [string]$LogDir
)

. (Join-Path $PSScriptRoot "common.ps1")

$Root = Split-Path -Parent $PSScriptRoot
if (-not $LogDir) { $LogDir = Join-Path $Root "logs" }
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

if (-not $ExpectJson -and $env:VERIFY_EXPECT_JSON) {
    $ExpectJson = $env:VERIFY_EXPECT_JSON
}
if (-not $ExpectRegexJson -and $env:VERIFY_EXPECT_REGEX_JSON) {
    $ExpectRegexJson = $env:VERIFY_EXPECT_REGEX_JSON
}

if ($ExpectJson) {
    try {
        $Expect = @()
        foreach ($item in ($ExpectJson | ConvertFrom-Json)) { $Expect += [string]$item }
    } catch { Write-Warn "ExpectJson is invalid; using defaults"; }
}
if ($ExpectRegexJson) {
    try {
        $ExpectRegex = @()
        foreach ($item in ($ExpectRegexJson | ConvertFrom-Json)) { $ExpectRegex += [string]$item }
    } catch { Write-Warn "ExpectRegexJson is invalid; using defaults"; }
}

$logPath  = Join-Path $LogDir "serial.log"
$jsonPath = Join-Path $LogDir "serial.json"

if ($ListPorts) {
    $ports = @(Get-SerialPorts)
    if ($ports.Count -eq 0) {
        Write-Host "No serial ports found."
    } else {
        $ports | Format-Table -AutoSize
    }
    exit 0
}

function Get-MissingPatterns([string]$Text, [string[]]$Literals, [string[]]$Regexes) {
    $missing = @()
    foreach ($p in @($Literals)) {
        if (-not $p) { continue }
        if (-not [regex]::IsMatch($Text, [regex]::Escape($p),
                [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)) {
            $missing += "literal:$p"
        }
    }
    foreach ($p in @($Regexes)) {
        if (-not $p) { continue }
        if (-not [regex]::IsMatch($Text, $p,
                [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)) {
            $missing += "regex:$p"
        }
    }
    return @($missing)
}

$resolved = $null
try {
    $resolved = Resolve-SerialPort $Port
} catch {
    Write-Err $_.Exception.Message
    Write-JsonFile $jsonPath ([ordered]@{
        ok = $false; error = $_.Exception.Message; log = $logPath
    })
    Write-Host "RESULT: SERIAL_FAIL reason=no_port"
    exit 1
}

Write-Step "Serial capture"
Write-Host "Port   : $resolved"
Write-Host "Baud   : $Baud"
Write-Host "Timeout: $TimeoutSec s"
Write-Host "Expect : $($Expect -join ' | ')"
Write-Host "Regex  : $($ExpectRegex -join ' | ')"

$sp = New-Object System.IO.Ports.SerialPort
$sp.PortName = $resolved
$sp.BaudRate = $Baud
$sp.Parity = [System.IO.Ports.Parity]::None
$sp.DataBits = 8
$sp.StopBits = [System.IO.Ports.StopBits]::One
$sp.Handshake = [System.IO.Ports.Handshake]::None
$sp.DtrEnable = $false
$sp.RtsEnable = $false
$sp.ReadTimeout = 250
$sp.WriteTimeout = 250

$sb = New-Object System.Text.StringBuilder
$resetRc = $null
$bytes = 0

try {
    try {
        $sp.Open()
    } catch {
        throw "Cannot open $resolved. Close any serial monitor or PuTTY holding it. $($_.Exception.Message)"
    }

    Start-Sleep -Milliseconds 150
    if ($Reset) {
        # Let the post-flash boot finish so its partial banner does not pollute
        # the evidence captured after our own reset.
        Start-Sleep -Milliseconds 1000
    }
    try { $sp.DiscardInBuffer(); $sp.DiscardOutBuffer() } catch { }

    if ($Reset) {
        $cli = Get-StLinkCli
        if ($cli) {
            Write-Host "Reset  : ST-LINK_CLI -Rst"
            & $cli -c SWD -Rst -Q -NoPrompt | Out-Null
            $resetRc = $LASTEXITCODE
            if ($resetRc -ne 0) {
                Write-Warn "ST-LINK reset returned $resetRc (probe busy?). Banner may be missed."
            }
        } else {
            Write-Warn "ST-LINK_CLI not found; cannot reset target. Press the board reset button."
        }
    }

    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    $allMatchedAt = $null

    while ((Get-Date) -lt $deadline) {
        try {
            $chunk = $sp.ReadExisting()
        } catch {
            break
        }
        if ($chunk) {
            [void]$sb.Append($chunk)
            $bytes += $chunk.Length
        }

        $text = $sb.ToString()
        $missing = @(Get-MissingPatterns $text $Expect $ExpectRegex)
        if ($missing.Count -eq 0) {
            if (-not $allMatchedAt) { $allMatchedAt = Get-Date }
            if (((Get-Date) - $allMatchedAt).TotalMilliseconds -ge 300) { break }
        } else {
            $allMatchedAt = $null
        }
        Start-Sleep -Milliseconds 50
    }
} catch {
    $errText = $_.Exception.Message
    Write-Err $errText
    Write-JsonFile $jsonPath ([ordered]@{
        ok = $false; port = $resolved; baud = $Baud; error = $errText; log = $logPath
    })
    Write-Host "RESULT: SERIAL_FAIL reason=open_or_read"
    exit 1
} finally {
    if ($sp.IsOpen) { $sp.Close() }
    $sp.Dispose()
}

$text = $sb.ToString()
$utf8 = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($logPath, $text, $utf8)

$missing = @(Get-MissingPatterns $text $Expect $ExpectRegex)
$ok = ($missing.Count -eq 0)

$result = [ordered]@{
    ok         = $ok
    port       = $resolved
    baud       = $Baud
    timeoutSec = $TimeoutSec
    reset      = [bool]$Reset
    resetExit  = $resetRc
    bytes      = $bytes
    expect     = @($Expect)
    expectRegex = @($ExpectRegex)
    missing    = @($missing)
    log        = $logPath
}
Write-JsonFile $jsonPath $result

Write-Host ""
Write-Host "---- serial output (tail) ----" -ForegroundColor DarkGray
$lines = @($text -split "`r?`n" | Where-Object { $_ -ne "" })
$lines | Select-Object -Last 25 | ForEach-Object { Write-Host $_ }

Write-Host ""
Write-Host ("Bytes : {0}   Log: {1}" -f $bytes, $logPath)

if ($ok) {
    Write-Ok "all expectations matched"
    Write-Host "RESULT: SERIAL_OK port=$resolved"
    exit 0
}

Write-Err "missing expectations"
$missing | ForEach-Object { Write-Host "  $_" }
Write-Host "RESULT: SERIAL_FAIL missing=$($missing.Count)"
exit 1


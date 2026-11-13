# common.ps1 -- shared helpers for the board verification gates.
# PowerShell 5.1 compatible. ASCII only: do not add non-ASCII text here.

$ErrorActionPreference = "Stop"

function Write-Step([string]$Text) {
    Write-Host ""
    Write-Host "==> $Text" -ForegroundColor Cyan
}

function Write-Ok([string]$Text) {
    Write-Host "OK   $Text" -ForegroundColor Green
}

function Write-Warn([string]$Text) {
    Write-Host "WARN $Text" -ForegroundColor Yellow
}

function Write-Err([string]$Text) {
    Write-Host "FAIL $Text" -ForegroundColor Red
}

function Write-JsonFile([string]$Path, $Object) {
    $dir = Split-Path -Parent $Path
    if ($dir -and -not (Test-Path $dir)) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
    }
    $text = $Object | ConvertTo-Json -Depth 8
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $text, $utf8)
}

function Read-JsonFile([string]$Path) {
    if (-not (Test-Path $Path)) { return $null }
    $raw = Get-Content -Path $Path -Raw -ErrorAction SilentlyContinue
    if (-not $raw) { return $null }
    return ($raw | ConvertFrom-Json)
}

function Get-StLinkCli {
    if ($env:STLINK_CLI -and (Test-Path $env:STLINK_CLI)) { return $env:STLINK_CLI }

    $candidates = @(
        "C:\Program Files (x86)\STMicroelectronics\STM32 ST-LINK Utility\ST-LINK Utility\ST-LINK_CLI.exe",
        "C:\Program Files\STMicroelectronics\STM32 ST-LINK Utility\ST-LINK Utility\ST-LINK_CLI.exe"
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { return $c }
    }

    $cmd = Get-Command ST-LINK_CLI.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    return $null
}

function Get-SerialPorts {
    $ports = @()
    try {
        $ports = Get-CimInstance Win32_PnPEntity -ErrorAction Stop |
                 Where-Object { $_.Name -match '(?i)\bCOM\d+\b' } |
                 ForEach-Object {
                     $m = [regex]::Match($_.Name, '(?i)\b(COM\d+)\b')
                     [pscustomobject]@{
                         Name        = $m.Groups[1].Value.ToUpper()
                         Description = $_.Name
                     }
                 }
    } catch {
        $ports = @()
    }

    if (-not $ports -or $ports.Count -eq 0) {
        $ports = [System.IO.Ports.SerialPort]::GetPortNames() |
                 ForEach-Object { [pscustomobject]@{ Name = $_; Description = $_ } }
    }
    return @($ports)
}

function Resolve-SerialPort([string]$Port) {
    $available = @(Get-SerialPorts)
    $names = @($available | ForEach-Object { $_.Name })

    if ($Port) {
        if ($names -notcontains $Port) {
            throw "Serial port $Port not found. Available: $($names -join ', ')"
        }
        return $Port
    }

    $usb = $available | Where-Object {
        $_.Description -match '(?i)(CH340|CH341|CP210|FTDI|USB[- ]?SERIAL|Silicon Labs|Prolific|ST-?Link|STMicro)'
    } | Select-Object -First 1
    if ($usb) { return $usb.Name }

    if ($names -contains "COM3") { return "COM3" }
    if ($names.Count -eq 1) { return $names[0] }

    throw "Cannot pick a serial port automatically. Available: $($names -join ', '). Use -Port COMx."
}

function Convert-ToWslPath([string]$WindowsPath) {
    $full = [System.IO.Path]::GetFullPath($WindowsPath)
    $drive = $full.Substring(0, 1).ToLower()
    $rest = $full.Substring(2).Replace('\', '/')
    return "/mnt/$drive$rest"
}

function Get-ElfSize([string]$ElfPath) {
    if (-not (Test-Path $ElfPath)) { return $null }

    $out = $null
    $sizeCmd = Get-Command arm-none-eabi-size.exe -ErrorAction SilentlyContinue
    if ($sizeCmd) {
        $out = & $sizeCmd.Source $ElfPath 2>&1
    } else {
        $wsl = Get-Command wsl.exe -ErrorAction SilentlyContinue
        if ($wsl) {
            $wslPath = Convert-ToWslPath $ElfPath
            $out = & wsl.exe -e arm-none-eabi-size $wslPath 2>&1
        }
    }

    if (-not $out) { return $null }

    foreach ($line in @($out)) {
        $m = [regex]::Match([string]$line, '^\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+')
        if ($m.Success) {
            $text = [int64]$m.Groups[1].Value
            $data = [int64]$m.Groups[2].Value
            $bss  = [int64]$m.Groups[3].Value
            return [pscustomobject]@{
                text  = $text
                data  = $data
                bss   = $bss
                flash = $text + $data
                ram   = $data + $bss
                raw   = ($out -join "`n")
            }
        }
    }
    return $null
}

function Get-GitCommit([string]$Root) {
    try {
        Push-Location $Root
        $c = & git rev-parse --short HEAD 2>$null
        if ($LASTEXITCODE -eq 0 -and $c) { return ([string]$c).Trim() }
    } catch {
    } finally {
        Pop-Location
    }
    return "unknown"
}

function Get-LogTail([string]$Path, [int]$Lines = 40) {
    if (-not (Test-Path $Path)) { return @() }
    return @(Get-Content -Path $Path -Tail $Lines -ErrorAction SilentlyContinue)
}

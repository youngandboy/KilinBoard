[CmdletBinding()]
param(
    [string]$Port = "COM3",
    [int]$SerialTimeoutSec = 25,
    [int]$ApiTimeoutSec = 30,
    [string]$ApiUrl = "http://127.0.0.1:8080/api/data",
    [string]$RttRoot = "/mnt/h/RTOS/rt-thread-5.3.0",
    [string]$Scons = "/home/yu/.local/bin/scons",
    [switch]$SkipBuild,
    [switch]$SkipFlash,
    [switch]$SkipSerial,
    [switch]$SkipApi,
    [switch]$NoReset
)

$ErrorActionPreference = "Stop"

$RepoDir = Split-Path -Parent $PSScriptRoot
$BspDir = Join-Path $RepoDir "bsp\stm32\stm32f103-kylin-v31"
$HexFile = Join-Path $BspDir "rt-thread.hex"
$BootHexFile = Join-Path $RepoDir "bootloader\build\bootloader.hex"
$FlashBat = Join-Path (Split-Path -Parent $RepoDir) "flash.bat"
$Stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$LogDir = Join-Path $RepoDir ("logs\rtthread-verify-" + $Stamp)
$script:Checks = @()

New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

function Write-Step {
    param([string]$Text)
    Write-Host ""
    Write-Host "=== $Text ==="
}

function Add-Check {
    param(
        [string]$Name,
        [bool]$Ok,
        [string]$Detail = ""
    )

    $script:Checks += [pscustomobject]@{
        Name = $Name
        Ok = $Ok
        Detail = $Detail
    }

    $state = if ($Ok) { "OK" } else { "FAIL" }
    Write-Host ("{0}: {1} {2}" -f $Name, $state, $Detail)
}

function Convert-ToWslPath {
    param([string]$Path)

    $value = & wsl.exe -e wslpath -a $Path 2>$null
    if (($LASTEXITCODE -ne 0) -or [string]::IsNullOrWhiteSpace($value)) {
        throw "wslpath failed for $Path"
    }
    return $value.Trim()
}

function Get-ImageSize {
    param([string]$WslRepo)

    $wslBsp = "$WslRepo/bsp/stm32/stm32f103-kylin-v31"
    $sizeOutput = & wsl.exe -e bash -lc "cd '$wslBsp' && arm-none-eabi-size rt-thread.elf" 2>&1
    if ($LASTEXITCODE -ne 0) {
        return $null
    }

    $line = $sizeOutput | Where-Object { $_ -match '^\s*\d+\s+\d+\s+\d+\s+.*rt-thread\.elf' } | Select-Object -Last 1
    if ($null -eq $line) {
        return $null
    }

    $parts = ($line -replace '^\s+', '') -split '\s+'
    if ($parts.Count -lt 3) {
        return $null
    }

    return @{
        Rom = [int]$parts[0]
        Ram = ([int]$parts[1] + [int]$parts[2])
        Output = ($sizeOutput | Out-String)
    }
}

function Find-StlinkCli {
    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($env:STLINK_CLI)) {
        $candidates += $env:STLINK_CLI
    }
    $candidates += @(
        "C:\Program Files (x86)\STMicroelectronics\STM32 ST-LINK Utility\ST-LINK Utility\ST-LINK_CLI.exe",
        "C:\Program Files\STMicroelectronics\STM32 ST-LINK Utility\ST-LINK Utility\ST-LINK_CLI.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    $command = Get-Command "ST-LINK_CLI.exe" -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    return $null
}

function Wait-ForApiData {
    param(
        [string]$Url,
        [int]$TimeoutSec
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    $lastError = ""
    $lastPayload = $null

    while ((Get-Date) -lt $deadline) {
        try {
            $payload = Invoke-RestMethod -Uri $Url -TimeoutSec 5
            $lastPayload = $payload

            if ($payload.connected -and ($null -ne $payload.last)) {
                $age = [math]::Max(0, [DateTimeOffset]::UtcNow.ToUnixTimeSeconds() - [double]$payload.last._recv)
                if ($age -le 30) {
                    return @{
                        Ok = $true
                        Payload = $payload
                        Detail = "seq=$($payload.last.seq) age=$([math]::Round($age, 1))s"
                    }
                }
                $lastError = "last data is stale ($([math]::Round($age, 1))s)"
            }
            else {
                $lastError = "board not connected or no data yet"
            }
        }
        catch {
            $lastError = $_.Exception.Message
        }

        Start-Sleep -Seconds 2
    }

    return @{
        Ok = $false
        Payload = $lastPayload
        Detail = $lastError
    }
}

try {
    Write-Host "RT-Thread verify: $RepoDir"
    Write-Host "Logs: $LogDir"

    # ------------------------------------------------------------------
    # Build + size
    # ------------------------------------------------------------------
    $wslRepo = Convert-ToWslPath $RepoDir

    if (-not $SkipBuild) {
        Write-Step "Build (WSL)"
        $buildCommand = "cd '$wslRepo' && RTT_ROOT='$RttRoot' SCONS='$Scons' bash build.sh"
        $buildOutput = & wsl.exe -e bash -lc $buildCommand 2>&1
        $buildExit = $LASTEXITCODE
        $buildLog = Join-Path $LogDir "build.log"
        $buildOutput | Out-File -FilePath $buildLog -Encoding utf8
        $buildOutput | ForEach-Object { Write-Host $_ }

        if ($buildExit -ne 0) {
            Add-Check "BUILD_OK" $false "build.sh exit=$buildExit (see build.log)"
            throw "build failed"
        }
        Add-Check "BUILD_OK" $true "see build.log"

        $buildText = ($buildOutput | Out-String)
        $sizeInfo = Get-ImageSize $wslRepo
        $rom = $null
        $ram = $null

        if ($null -ne $sizeInfo) {
            $rom = $sizeInfo.Rom
            $ram = $sizeInfo.Ram
            $sizeInfo.Output | Out-File -FilePath (Join-Path $LogDir "size.txt") -Encoding utf8
        }
        else {
            $romMatch = [regex]::Match($buildText, "ROM:\s+(\d+)\s+B")
            $ramMatch = [regex]::Match($buildText, "RAM:\s+(\d+)\s+B")
            if ($romMatch.Success -and $ramMatch.Success) {
                $rom = [int]$romMatch.Groups[1].Value
                $ram = [int]$ramMatch.Groups[1].Value
                "ROM=$rom B / 524288 B`nRAM=$ram B / 65536 B" |
                    Out-File -FilePath (Join-Path $LogDir "size.txt") -Encoding utf8
            }
        }

        if (($null -ne $rom) -and ($null -ne $ram)) {
            $sizeOk = (($rom -le 237568) -and ($ram -le 65536))
            Add-Check "SIZE_OK" $sizeOk "ROM=$rom B / 232 KB, RAM=$ram B"
        }
        else {
            Add-Check "SIZE_OK" $false "could not parse ROM/RAM from ELF or build output"
        }
    }
    else {
        Add-Check "BUILD_OK" $true "skipped"
        Add-Check "SIZE_OK" $true "skipped"
    }

    # ------------------------------------------------------------------
    # Flash
    # ------------------------------------------------------------------
    if (-not $SkipFlash) {
        Write-Step "Flash"
        if (-not (Test-Path $HexFile)) {
            Add-Check "FLASH_OK" $false "app HEX not found: $HexFile"
            throw "missing app HEX"
        }
        if (-not (Test-Path $BootHexFile)) {
            Add-Check "FLASH_OK" $false "bootloader HEX not found: $BootHexFile"
            throw "missing bootloader HEX"
        }
        if (-not (Test-Path $FlashBat)) {
            Add-Check "FLASH_OK" $false "flash.bat not found: $FlashBat"
            throw "missing flash.bat"
        }

        $bootFlashOutput = & $FlashBat $BootHexFile 2>&1
        $bootFlashExit = $LASTEXITCODE
        $bootFlashOutput | Out-File -FilePath (Join-Path $LogDir "flash-boot.log") -Encoding utf8
        $bootFlashOutput | ForEach-Object { Write-Host $_ }
        if (($bootFlashExit -ne 0) -or (($bootFlashOutput | Out-String) -notmatch "Flash OK")) {
            Add-Check "FLASH_OK" $false "bootloader flash exit=$bootFlashExit"
            throw "bootloader flash failed"
        }

        $flashOutput = & $FlashBat $HexFile 2>&1
        $flashExit = $LASTEXITCODE
        $flashOutput | Out-File -FilePath (Join-Path $LogDir "flash.log") -Encoding utf8
        $flashOutput | ForEach-Object { Write-Host $_ }

        if (($flashExit -ne 0) -or (($flashOutput | Out-String) -notmatch "Flash OK")) {
            Add-Check "FLASH_OK" $false "app flash exit=$flashExit (see flash.log)"
            throw "app flash failed"
        }
        Add-Check "FLASH_OK" $true "bootloader + app"
    }
    else {
        Add-Check "FLASH_OK" $true "skipped"
    }

    # ------------------------------------------------------------------
    # Serial banner / application start / sensor
    # ------------------------------------------------------------------
    if (-not $SkipSerial) {
        Write-Step "Serial $Port"
        $ports = [System.IO.Ports.SerialPort]::GetPortNames()
        if ($ports -notcontains $Port) {
            Add-Check "SERIAL_OK" $false "port $Port not found (available: $($ports -join ', '))"
            throw "serial port missing"
        }

        $serial = New-Object System.IO.Ports.SerialPort $Port,115200,None,8,One
        $serial.ReadTimeout = 500
        $serialText = ""

        try {
            $serial.Open()
            Start-Sleep -Milliseconds 100
            $serial.DiscardInBuffer()

            if (-not $NoReset) {
                $cli = Find-StlinkCli
                if ($null -eq $cli) {
                    Add-Check "RESET_OK" $false "ST-LINK_CLI.exe not found"
                    throw "ST-LINK CLI missing"
                }

                $resetOutput = & $cli -c SWD -Rst -Run -Q 2>&1
                $resetExit = $LASTEXITCODE
                $resetOutput | Out-File -FilePath (Join-Path $LogDir "reset.log") -Encoding utf8
                if ($resetExit -ne 0) {
                    Add-Check "RESET_OK" $false "reset exit=$resetExit"
                    throw "reset failed"
                }
                Add-Check "RESET_OK" $true "ST-Link reset"
            }
            else {
                Add-Check "RESET_OK" $true "skipped"
            }

            $watch = [Diagnostics.Stopwatch]::StartNew()
            while ($watch.Elapsed.TotalSeconds -lt $SerialTimeoutSec) {
                $chunk = $serial.ReadExisting()
                if (-not [string]::IsNullOrEmpty($chunk)) {
                    $serialText += $chunk
                    Write-Host -NoNewline $chunk
                }

                if (($serialText -match "Thread Operating System") -and
                    ($serialText -match "\[app\] started") -and
                    ($serialText -match "\[sensor\]")) {
                    break
                }

                Start-Sleep -Milliseconds 200
            }
        }
        catch {
            if ($null -ne $serial -and $serial.IsOpen) {
                $serial.Close()
            }
            throw
        }
        finally {
            if ($null -ne $serial -and $serial.IsOpen) {
                $serial.Close()
            }
        }

        $serialText | Out-File -FilePath (Join-Path $LogDir "serial.log") -Encoding utf8
        Write-Host ""

        $hasBanner = $serialText -match "Thread Operating System"
        $hasApp = $serialText -match "\[app\] started"
        $hasSensor = $serialText -match "\[sensor\]"
        $serialOk = if ($NoReset) {
            $hasSensor
        }
        else {
            $hasBanner -and $hasApp -and $hasSensor
        }

        Add-Check "SERIAL_OK" $serialOk "banner=$hasBanner app=$hasApp sensor=$hasSensor no_reset=$NoReset"
        if (-not $serialOk) {
            throw "serial assertions failed"
        }
    }
    else {
        Add-Check "SERIAL_OK" $true "skipped"
        Add-Check "RESET_OK" $true "skipped"
    }

    # ------------------------------------------------------------------
    # HTTP API / live telemetry
    # ------------------------------------------------------------------
    if (-not $SkipApi) {
        Write-Step "API $ApiUrl"
        $api = Wait-ForApiData -Url $ApiUrl -TimeoutSec $ApiTimeoutSec
        if ($null -ne $api.Payload) {
            $api.Payload | ConvertTo-Json -Depth 8 |
                Out-File -FilePath (Join-Path $LogDir "api.json") -Encoding utf8
        }

        Add-Check "API_OK" $api.Ok $api.Detail
        if (-not $api.Ok) {
            throw "API assertion failed"
        }
    }
    else {
        Add-Check "API_OK" $true "skipped"
    }
}
catch {
    Add-Check "ERROR" $false $_.Exception.Message
}
finally {
    $failed = @($script:Checks | Where-Object { -not $_.Ok })
    $pass = ($failed.Count -eq 0)

    $summary = @()
    $summary += "# RT-Thread verify summary"
    $summary += ""
    $summary += "- Timestamp: $Stamp"
    $summary += "- Port: $Port"
    $summary += "- Result: " + $(if ($pass) { "PASS" } else { "FAIL" })
    $summary += ""
    $summary += "| Check | Result | Detail |"
    $summary += "| --- | --- | --- |"
    foreach ($check in $script:Checks) {
        $state = if ($check.Ok) { "OK" } else { "FAIL" }
        $summary += "| $($check.Name) | $state | $($check.Detail) |"
    }
    $summary += ""
    $summary += "Logs: $LogDir"

    $summary | Out-File -FilePath (Join-Path $LogDir "summary.md") -Encoding utf8

    Write-Host ""
    Write-Host "Evidence: $LogDir"
    if ($pass) {
        Write-Host "RESULT: PASS"
        exit 0
    }

    Write-Host "RESULT: FAIL"
    exit 1
}

[CmdletBinding()]
param(
    [string]$Port = "COM3",
    [uint32]$Version = 0x1234,
    [string]$RttRoot = "/mnt/h/RTOS/rt-thread-5.3.0",
    [string]$Scons = "/home/yu/.local/bin/scons",
    [string]$ApiUrl = "http://127.0.0.1:8080/api/data",
    [int]$ApiTimeoutSec = 45,
    [switch]$SkipBuild,
    [switch]$SkipFlash
)

$ErrorActionPreference = "Stop"

$RepoDir = Split-Path -Parent $PSScriptRoot
$BspDir = Join-Path $RepoDir "bsp\stm32\stm32f103-kylin-v31"
$BinFile = Join-Path $BspDir "rtthread.bin"
$AppHex = Join-Path $BspDir "rt-thread.hex"
$BootHex = Join-Path $RepoDir "bootloader\build\bootloader.hex"
$FlashBat = Join-Path (Split-Path -Parent $RepoDir) "flash.bat"
$PackScript = Join-Path $PSScriptRoot "ota_pack.py"
$SendScript = Join-Path $PSScriptRoot "ota_send.ps1"
$Stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$LogDir = Join-Path $RepoDir ("logs\rtthread-ota-" + $Stamp)
$OtaFile = Join-Path $LogDir ("rtthread-" + $Version.ToString("x8") + ".ota")
$script:Checks = @()

New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

function Add-Check {
    param([string]$Name, [bool]$Ok, [string]$Detail = "")
    $script:Checks += [pscustomobject]@{ Name = $Name; Ok = $Ok; Detail = $Detail }
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

try {
    Write-Host "RT-Thread OTA verify: $RepoDir"
    Write-Host "Logs: $LogDir"

    if (-not $SkipBuild) {
        Write-Host ""
        Write-Host "=== Build ==="
        $wslRepo = Convert-ToWslPath $RepoDir
        $cmd = "cd '$wslRepo' && RTT_ROOT='$RttRoot' SCONS='$Scons' bash build.sh"
        $buildOut = & wsl.exe -e bash -lc $cmd 2>&1
        $buildExit = $LASTEXITCODE
        $buildOut | Out-File -FilePath (Join-Path $LogDir "build.log") -Encoding utf8
        $buildOut | ForEach-Object { Write-Host $_ }
        if ($buildExit -ne 0) {
            Add-Check "BUILD_OK" $false "build.sh exit=$buildExit"
            throw "build failed"
        }
        Add-Check "BUILD_OK" $true "bootloader + app"
    }
    else {
        Add-Check "BUILD_OK" $true "skipped"
    }

    if (-not $SkipFlash) {
        Write-Host ""
        Write-Host "=== Flash bootloader + app ==="
        foreach ($item in @(
            @{ Name = "bootloader"; Hex = $BootHex; Log = "flash-boot.log" },
            @{ Name = "app"; Hex = $AppHex; Log = "flash-app.log" }
        )) {
            if (-not (Test-Path $item.Hex)) {
                Add-Check "FLASH_OK" $false "$($item.Name) HEX not found: $($item.Hex)"
                throw "missing $($item.Name) HEX"
            }
            $out = & $FlashBat $item.Hex 2>&1
            $exit = $LASTEXITCODE
            $out | Out-File -FilePath (Join-Path $LogDir $item.Log) -Encoding utf8
            $out | ForEach-Object { Write-Host $_ }
            if (($exit -ne 0) -or (($out | Out-String) -notmatch "Flash OK")) {
                Add-Check "FLASH_OK" $false "$($item.Name) flash exit=$exit"
                throw "$($item.Name) flash failed"
            }
        }
        Add-Check "FLASH_OK" $true "bootloader + app"
        Start-Sleep -Seconds 8
    }
    else {
        Add-Check "FLASH_OK" $true "skipped"
    }

    if (-not (Test-Path $BinFile)) {
        Add-Check "PACK_OK" $false "rtthread.bin not found"
        throw "missing rtthread.bin"
    }

    Write-Host ""
    Write-Host "=== Package ==="
    $packOut = & python $PackScript $BinFile -o $OtaFile --version $Version 2>&1
    $packExit = $LASTEXITCODE
    $packOut | Out-File -FilePath (Join-Path $LogDir "pack.log") -Encoding utf8
    $packOut | ForEach-Object { Write-Host $_ }
    if (($packExit -ne 0) -or -not (Test-Path $OtaFile)) {
        Add-Check "PACK_OK" $false "ota_pack.py exit=$packExit"
        throw "pack failed"
    }
    Add-Check "PACK_OK" $true $OtaFile

    Write-Host ""
    Write-Host "=== Send OTA ==="
    $otaOut = & powershell -NoProfile -ExecutionPolicy Bypass -File $SendScript `
        -Port $Port -Image $OtaFile 2>&1
    $otaExit = $LASTEXITCODE
    $otaText = ($otaOut | Out-String)
    $otaOut | Out-File -FilePath (Join-Path $LogDir "ota.log") -Encoding utf8
    $otaOut | ForEach-Object { Write-Host $_ }
    if (($otaExit -ne 0) -or ($otaText -notmatch "\[boot\] update OK")) {
        Add-Check "OTA_OK" $false "update did not report success"
        throw "OTA failed"
    }
    Add-Check "OTA_OK" $true "bootloader copied image"

    Write-Host ""
    Write-Host "=== Verify app ==="
    $serial = New-Object System.IO.Ports.SerialPort $Port,115200,None,8,One
    $serial.ReadTimeout = 500
    $serialText = ""
    try {
        $serial.Open()
        Start-Sleep -Milliseconds 200
        $serial.DiscardInBuffer()
        $cmd = [System.Text.Encoding]::ASCII.GetBytes("ota status`r`n")
        $serial.Write($cmd, 0, $cmd.Length)
        $watch = [Diagnostics.Stopwatch]::StartNew()
        while ($watch.Elapsed.TotalSeconds -lt 5) {
            $chunk = $serial.ReadExisting()
            if (-not [string]::IsNullOrEmpty($chunk)) {
                $serialText += $chunk
            }
            if ($serialText -match "version=0x([0-9a-fA-F]+)") {
                break
            }
            Start-Sleep -Milliseconds 100
        }
    }
    finally {
        if ($serial.IsOpen) { $serial.Close() }
    }
    $serialText | Out-File -FilePath (Join-Path $LogDir "serial.log") -Encoding utf8
    $m = [regex]::Match($serialText, "version=0x([0-9a-fA-F]+)")
    $versionOk = $m.Success -and ([Convert]::ToUInt32($m.Groups[1].Value, 16) -eq $Version)
    Add-Check "VERSION_OK" $versionOk ("expected=0x{0:x8}" -f $Version)

    $deadline = (Get-Date).AddSeconds($ApiTimeoutSec)
    $apiOk = $false
    $apiPayload = $null
    while ((Get-Date) -lt $deadline) {
        try {
            $apiPayload = Invoke-RestMethod -Uri $ApiUrl -TimeoutSec 5
            if ($apiPayload.connected -and ($null -ne $apiPayload.last)) {
                $age = [math]::Max(0, [DateTimeOffset]::UtcNow.ToUnixTimeSeconds() - [double]$apiPayload.last._recv)
                if ($age -le 30) {
                    $apiOk = $true
                    break
                }
            }
        }
        catch {
        }
        Start-Sleep -Seconds 2
    }
    if ($null -ne $apiPayload) {
        $apiPayload | ConvertTo-Json -Depth 8 |
            Out-File -FilePath (Join-Path $LogDir "api.json") -Encoding utf8
    }
    Add-Check "API_OK" $apiOk "live telemetry after OTA"
    if (-not $apiOk) {
        throw "API check failed"
    }
}
catch {
    Add-Check "ERROR" $false $_.Exception.Message
}
finally {
    $failed = @($script:Checks | Where-Object { -not $_.Ok })
    $pass = ($failed.Count -eq 0)

    $summary = @()
    $summary += "# RT-Thread OTA verify summary"
    $summary += ""
    $summary += "- Timestamp: $Stamp"
    $summary += "- Port: $Port"
    $summary += ("- Version: 0x{0:x8}" -f $Version)
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

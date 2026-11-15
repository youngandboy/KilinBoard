param(
    [int]$Hours = 24,
    [int]$IntervalSec = 10,
    [string]$OutFile = ""
)

if ([string]::IsNullOrWhiteSpace($OutFile)) {
    $OutFile = Join-Path $env:TEMP ("kylin-soak-" + (Get-Date -Format "yyyyMMdd-HHmmss") + ".csv")
}

$end = (Get-Date).AddHours($Hours)
Write-Host "Soak test started: $Hours h, every $IntervalSec s"
Write-Host "Log: $OutFile"

while ((Get-Date) -lt $end) {
    $now = Get-Date
    $ok = 0
    $connected = 0
    $seq = ""
    $up = ""
    $temp = ""
    $humi = ""
    $light = ""
    $age = ""
    $stale = ""
    $errorText = ""

    try {
        $d = Invoke-RestMethod -Uri "http://127.0.0.1:8080/api/data" -TimeoutSec 5
        $ok = 1
        $connected = if ($d.connected) { 1 } else { 0 }
        if ($null -ne $d.last) {
            $last = $d.last
            $seq = $last.seq
            $up = $last.up
            $temp = $last.temp
            $humi = $last.humi
            $light = $last.light
            $age = [math]::Round([DateTimeOffset]::UtcNow.ToUnixTimeSeconds() - $last._recv, 1)
            $stale = if ($age -gt 15) { 1 } else { 0 }
        }
    }
    catch {
        $errorText = $_.Exception.Message
    }

    $row = [pscustomobject]@{
        time = $now.ToString("yyyy-MM-dd HH:mm:ss")
        ok = $ok
        connected = $connected
        seq = $seq
        up = $up
        temp = $temp
        humi = $humi
        light = $light
        age_s = $age
        stale = $stale
        error = $errorText
    }

    if (-not (Test-Path $OutFile)) {
        $row | Export-Csv -Path $OutFile -NoTypeInformation -Encoding UTF8
    } else {
        $row | Export-Csv -Path $OutFile -Append -NoTypeInformation -Encoding UTF8
    }

    Start-Sleep -Seconds $IntervalSec
}

Write-Host "Soak test finished: $OutFile"

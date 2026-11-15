param(
    [string]$Port = "COM3",
    [Parameter(Mandatory = $true)][string]$Image,
    [switch]$NoTrigger,
    [int]$HandshakeTimeoutSec = 20,
    [int]$PacketTimeoutMs = 5000,
    [switch]$KeepOpen
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $Image)) {
    throw "image not found: $Image"
}

function Get-Crc16([byte[]]$Data) {
    [int]$crc = 0
    foreach ($b in $Data) {
        $crc = ($crc -bxor ([int]$b -shl 8)) -band 0xFFFF
        for ($i = 0; $i -lt 8; $i++) {
            if (($crc -band 0x8000) -ne 0) {
                $crc = ((($crc -shl 1) -bxor 0x1021)) -band 0xFFFF
            }
            else {
                $crc = ($crc -shl 1) -band 0xFFFF
            }
        }
    }
    return [int]$crc
}

function New-Packet([byte]$Seq, [byte[]]$Data) {
    $packet = New-Object byte[] (3 + $Data.Length + 2)
    $packet[0] = if ($Data.Length -eq 1024) { [byte]0x02 } else { [byte]0x01 }
    $packet[1] = $Seq
    $packet[2] = [byte](0xFF - [int]$Seq)
    [Array]::Copy($Data, 0, $packet, 3, $Data.Length)
    $crc = Get-Crc16 $Data
    $packet[3 + $Data.Length] = [byte](($crc -shr 8) -band 0xFF)
    $packet[4 + $Data.Length] = [byte]($crc -band 0xFF)
    return $packet
}

function Read-One([System.IO.Ports.SerialPort]$Sp, [int]$TimeoutMs) {
    $Sp.ReadTimeout = $TimeoutMs
    try {
        return $Sp.ReadByte()
    }
    catch [System.TimeoutException] {
        return -1
    }
}

function Wait-ForByte([System.IO.Ports.SerialPort]$Sp, [int]$Wanted, [int]$TimeoutMs) {
    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    while ((Get-Date) -lt $deadline) {
        $b = Read-One $Sp 250
        if ($b -eq $Wanted) {
            return $true
        }
    }
    return $false
}

function Read-Ack([System.IO.Ports.SerialPort]$Sp, [int]$TimeoutMs) {
    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    while ((Get-Date) -lt $deadline) {
        $b = Read-One $Sp 250
        if ($b -eq 0x06) { return "ACK" }
        if ($b -eq 0x15) { return "NAK" }
        if ($b -eq 0x18) { return "CAN" }
    }
    return "TIMEOUT"
}

function Send-Packet([System.IO.Ports.SerialPort]$Sp, [byte[]]$Packet) {
    for ($attempt = 0; $attempt -lt 10; $attempt++) {
        $Sp.Write($Packet, 0, $Packet.Length)
        $r = Read-Ack $Sp $PacketTimeoutMs
        if ($r -eq "ACK") {
            return
        }
        if ($r -eq "CAN") {
            throw "receiver cancelled transfer"
        }
        Write-Host ("  packet retry ({0})" -f $r)
    }
    throw "packet send failed after retries"
}

$data = [System.IO.File]::ReadAllBytes((Resolve-Path $Image).Path)
$name = [System.Text.Encoding]::ASCII.GetBytes("rtthread.ota")
if ($name.Length -gt 100) {
    throw "filename too long"
}

$sp = New-Object System.IO.Ports.SerialPort $Port,115200,None,8,One
$sp.ReadTimeout = 500
$sp.WriteTimeout = 5000

try {
    $sp.Open()
    Start-Sleep -Milliseconds 200
    $sp.DiscardInBuffer()

    if (-not $NoTrigger) {
        Write-Host "Sending 'ota' command..."
        $cmd = [System.Text.Encoding]::ASCII.GetBytes("ota`r`n")
        $sp.Write($cmd, 0, $cmd.Length)
    }

    Write-Host "Waiting for Ymodem receiver..."
    $deadline = (Get-Date).AddSeconds($HandshakeTimeoutSec)
    $gotC = $false
    while ((Get-Date) -lt $deadline) {
        if (Wait-ForByte $sp 0x43 500) {
            $gotC = $true
            break
        }
    }
    if (-not $gotC) {
        throw "no Ymodem 'C' from bootloader"
    }

    $header = New-Object byte[] 128
    for ($i = 0; $i -lt $header.Length; $i++) { $header[$i] = 0 }
    [Array]::Copy($name, 0, $header, 0, $name.Length)
    $pos = $name.Length + 1
    $sizeText = [System.Text.Encoding]::ASCII.GetBytes($data.Length.ToString())
    [Array]::Copy($sizeText, 0, $header, $pos, $sizeText.Length)
    $header[$pos + $sizeText.Length] = 0

    Write-Host ("Sending header: {0} bytes" -f $data.Length)
    Send-Packet $sp (New-Packet ([byte]0) $header)
    if (-not (Wait-ForByte $sp 0x43 3000)) {
        Write-Host "  warning: no second 'C' after header"
    }

    $blockSize = 1024
    $seq = 1
    for ($offset = 0; $offset -lt $data.Length; $offset += $blockSize) {
        $chunk = [Math]::Min($blockSize, $data.Length - $offset)
        $block = New-Object byte[] $blockSize
        for ($i = 0; $i -lt $block.Length; $i++) { $block[$i] = 0x1A }
        [Array]::Copy($data, $offset, $block, 0, $chunk)
        Send-Packet $sp (New-Packet ([byte]$seq) $block)
        $seq = ($seq + 1) -band 0xFF
        if (($seq % 32) -eq 0) {
            Write-Host ("  sent {0}/{1} bytes" -f ([Math]::Min($offset + $chunk, $data.Length)), $data.Length)
        }
    }

    Write-Host "Sending EOT..."
    $eot = [byte[]]@(0x04)
    $sp.Write($eot, 0, 1)
    if ((Read-Ack $sp $PacketTimeoutMs) -ne "ACK") {
        throw "EOT not acknowledged"
    }
    if (-not (Wait-ForByte $sp 0x43 3000)) {
        Write-Host "  warning: no 'C' after EOT"
    }

    $empty = New-Object byte[] 128
    for ($i = 0; $i -lt $empty.Length; $i++) { $empty[$i] = 0 }
    Send-Packet $sp (New-Packet ([byte]0) $empty)
    Write-Host "Transfer complete; waiting for bootloader copy..."

    $watch = [Diagnostics.Stopwatch]::StartNew()
    while ($watch.Elapsed.TotalSeconds -lt 15) {
        if ($sp.BytesToRead -gt 0) {
            Write-Host -NoNewline $sp.ReadExisting()
        }
        Start-Sleep -Milliseconds 100
    }
    Write-Host ""
}
finally {
    if ($KeepOpen -and $sp.IsOpen) {
        # leave the port open only for an interactive caller
    }
    elseif ($sp.IsOpen) {
        $sp.Close()
    }
}

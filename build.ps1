param(
    [switch]$Clean,
    [switch]$Release,
    [string]$Toolchain
)

$ErrorActionPreference = "Stop"

$Root      = $PSScriptRoot
$BuildDir  = Join-Path $Root "build"
$ObjDir    = Join-Path $BuildDir "obj"
$OutDir    = Join-Path $Root "output\gcc"
$ElfFile   = Join-Path $OutDir "OneNet_Mini_Bare.elf"
$HexFile   = Join-Path $OutDir "OneNet_Mini_Bare.hex"
$BinFile   = Join-Path $OutDir "OneNet_Mini_Bare.bin"
$MapFile   = Join-Path $OutDir "OneNet_Mini_Bare.map"
$Linker    = Join-Path $Root "linker\STM32F103CB.ld"

function Write-Step([string]$Text) {
    Write-Host ""
    Write-Host "==> $Text" -ForegroundColor Cyan
}

function Fail([string]$Text) {
    Write-Host "ERROR: $Text" -ForegroundColor Red
    exit 1
}

function Find-ToolchainBin {
    if ($Toolchain) {
        if (Test-Path (Join-Path $Toolchain "arm-none-eabi-gcc.exe")) {
            return (Resolve-Path $Toolchain).Path
        }
        Fail "Toolchain path does not contain arm-none-eabi-gcc.exe: $Toolchain"
    }

    $cmd = Get-Command arm-none-eabi-gcc.exe -ErrorAction SilentlyContinue
    if ($cmd) {
        return (Split-Path $cmd.Source -Parent)
    }

    $candidates = @(
        (Join-Path $env:LOCALAPPDATA "ArmGnu\bin"),
        (Join-Path $env:USERPROFILE "ArmGnu\bin"),
        "C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin",
        "C:\Program Files\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin",
        "C:\Program Files (x86)\GNU Arm Embedded Toolchain\10 2021.10\bin",
        "C:\Program Files (x86)\GNU Arm Embedded Toolchain\9 2020-q2-update\bin"
    )

    foreach ($c in $candidates) {
        if (Test-Path (Join-Path $c "arm-none-eabi-gcc.exe")) {
            return $c
        }
    }

    # Search versioned install directories.
    $roots = @(
        "C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi",
        "C:\Program Files\Arm GNU Toolchain arm-none-eabi",
        "C:\Program Files (x86)\GNU Arm Embedded Toolchain",
        "C:\Program Files\GNU Arm Embedded Toolchain"
    )
    foreach ($r in $roots) {
        if (Test-Path $r) {
            $found = Get-ChildItem $r -Directory -ErrorAction SilentlyContinue |
                     Sort-Object Name -Descending |
                     ForEach-Object { Join-Path $_.FullName "bin" } |
                     Where-Object { Test-Path (Join-Path $_ "arm-none-eabi-gcc.exe") } |
                     Select-Object -First 1
            if ($found) { return $found }
        }
    }

    return $null
}

function Get-WslPath([string]$WindowsPath) {
    $p = (Resolve-Path $WindowsPath).Path
    $drive = $p.Substring(0, 1).ToLower()
    $rest = $p.Substring(2).Replace('\', '/')
    return "/mnt/$drive$rest"
}

function Invoke-WslBuild {
    $wsl = Get-Command wsl.exe -ErrorAction SilentlyContinue
    if (-not $wsl) { return $false }

    & wsl.exe -e bash -lc "command -v arm-none-eabi-gcc >/dev/null 2>&1 && command -v make >/dev/null 2>&1" | Out-Null
    if ($LASTEXITCODE -ne 0) { return $false }

    $wslPath = Get-WslPath $Root
    Write-Host "Toolchain: WSL ($wslPath)" -ForegroundColor DarkGray

    if ($Clean) {
        & wsl.exe -e make -C $wslPath clean | Out-Host
        if ($LASTEXITCODE -ne 0) { Fail "WSL make clean failed" }
    }

    $makeArgs = @("-e", "make", "-C", $wslPath)
    if ($Release) { $makeArgs += "RELEASE=1" }

    & wsl.exe @makeArgs | Out-Host
    if ($LASTEXITCODE -ne 0) { Fail "WSL make failed" }
    return $true
}

if ($Clean) {
    Write-Step "Cleaning build and output artifacts"
    Remove-Item $BuildDir -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $OutDir "OneNet_Mini_Bare.elf") -Force -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $OutDir "OneNet_Mini_Bare.hex") -Force -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $OutDir "OneNet_Mini_Bare.bin") -Force -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $OutDir "OneNet_Mini_Bare.map") -Force -ErrorAction SilentlyContinue
}

$ToolchainBin = Find-ToolchainBin

if (-not $ToolchainBin) {
    if (Invoke-WslBuild) {
        Write-Host ""
        Write-Host "Build OK (WSL)" -ForegroundColor Green
        exit 0
    }

    Fail @"
arm-none-eabi-gcc.exe not found on Windows, and WSL does not have arm-none-eabi-gcc/make.
Install one of:
  winget install Arm.GnuArmEmbeddedToolchain
  (WSL) sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi make
or pass -Toolchain <...\bin>.
"@
}

$Gcc      = Join-Path $ToolchainBin "arm-none-eabi-gcc.exe"
$Objcopy  = Join-Path $ToolchainBin "arm-none-eabi-objcopy.exe"
$SizeTool = Join-Path $ToolchainBin "arm-none-eabi-size.exe"

New-Item -ItemType Directory -Force -Path $ObjDir | Out-Null
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

Write-Host "Toolchain: $ToolchainBin" -ForegroundColor DarkGray
Write-Host "Root     : $Root" -ForegroundColor DarkGray

$Sources = @(
    "core\core_cm3.c",
    "core\system_stm32f10x.c",
    "core\stm32f10x_it.c",
    "core\syscalls.c",
    "fwlib\src\misc.c",
    "fwlib\src\stm32f10x_gpio.c",
    "fwlib\src\stm32f10x_rcc.c",
    "fwlib\src\stm32f10x_usart.c",
    "hardware\src\delay.c",
    "hardware\src\usart.c",
    "user\main.c"
)

$AssemblySources = @(
    "startup\startup_stm32f103xb.s"
)

$Optimization = if ($Release) { @("-Os", "-g0") } else { @("-O0", "-g3") }

$CommonFlags = @(
    "-mcpu=cortex-m3",
    "-mthumb",
    "-mfloat-abi=soft",
    "-DSTM32F10X_MD",
    "-DUSE_STDPERIPH_DRIVER",
    "-ffunction-sections",
    "-fdata-sections",
    "-Wall"
) + $Optimization + @(
    "-I$(Join-Path $Root 'core')",
    "-I$(Join-Path $Root 'fwlib\inc')",
    "-I$(Join-Path $Root 'hardware\inc')"
)

$Objects = @()

foreach ($rel in $Sources) {
    $src = Join-Path $Root $rel
    if (-not (Test-Path $src)) { Fail "Source file not found: $rel" }
    $name = ($rel -replace '[\\/]', '_') -replace '\.c$', '.o'
    $obj = Join-Path $ObjDir $name

    Write-Step "CC  $rel"
    & $Gcc @CommonFlags -c $src -o $obj
    if ($LASTEXITCODE -ne 0) { Fail "Compile failed: $rel" }
    $Objects += $obj
}

foreach ($rel in $AssemblySources) {
    $src = Join-Path $Root $rel
    if (-not (Test-Path $src)) { Fail "Startup file not found: $rel" }
    $name = ($rel -replace '[\\/]', '_') -replace '\.s$', '.o'
    $obj = Join-Path $ObjDir $name

    Write-Step "AS  $rel"
    & $Gcc @CommonFlags -x assembler-with-cpp -c $src -o $obj
    if ($LASTEXITCODE -ne 0) { Fail "Assemble failed: $rel" }
    $Objects += $obj
}

Write-Step "LD  OneNet_Mini_Bare.elf"
& $Gcc @CommonFlags -T $Linker `
    "-Wl,--gc-sections" `
    "-Wl,-Map=$MapFile" `
    "-specs=nano.specs" `
    "-specs=nosys.specs" `
    -o $ElfFile `
    @Objects
if ($LASTEXITCODE -ne 0) { Fail "Link failed" }

Write-Step "HEX/BIN"
& $Objcopy -O ihex $ElfFile $HexFile
if ($LASTEXITCODE -ne 0) { Fail "objcopy ihex failed" }
& $Objcopy -O binary $ElfFile $BinFile
if ($LASTEXITCODE -ne 0) { Fail "objcopy binary failed" }

Write-Step "SIZE"
& $SizeTool $ElfFile

Write-Host ""
Write-Host "Build OK" -ForegroundColor Green
Write-Host "  ELF: $ElfFile"
Write-Host "  HEX: $HexFile"
Write-Host "  BIN: $BinFile"

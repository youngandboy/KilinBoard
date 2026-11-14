#!/usr/bin/env bash
# Fetch RT-Thread v5.3.0 and the STM32F1 HAL/CMSIS packages needed by the BSP.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
RTT_ROOT="${RTT_ROOT:-/mnt/h/RTOS/rt-thread-5.3.0}"
BSP="$HERE/bsp/stm32/stm32f103-kylin-v31"

if [ ! -d "$RTT_ROOT" ]; then
    echo "Cloning RT-Thread v5.3.0 into $RTT_ROOT ..."
    git clone --depth 1 --branch v5.3.0 https://gitee.com/mirrors/rt-thread.git "$RTT_ROOT"
fi

mkdir -p "$BSP/packages"

clone_if_missing() {
    local url="$1"
    local dest="$2"
    if [ ! -d "$dest" ]; then
        echo "Cloning $(basename "$dest") ..."
        git clone --depth 1 "$url" "$dest"
    fi
}

clone_if_missing \
    https://github.com/RT-Thread-packages/CMSIS-Core.git \
    "$BSP/packages/CMSIS-Core-latest"
clone_if_missing \
    https://github.com/RT-Thread-packages/cmsis-device-f1.git \
    "$BSP/packages/stm32f1_cmsis_driver-latest"
clone_if_missing \
    https://github.com/RT-Thread-packages/stm32f1xx-hal-driver.git \
    "$BSP/packages/stm32f1_hal_driver-latest"

echo "RT-Thread root: $RTT_ROOT"
echo "BSP:            $BSP"
echo "Done."

#!/usr/bin/env bash
# Build the kyLin-V3.1 RT-Thread BSP in WSL.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
RTT_ROOT="${RTT_ROOT:-/mnt/h/RTOS/rt-thread-5.3.0}"
RTT_EXEC_PATH="${RTT_EXEC_PATH:-/usr/bin}"
SCONS="${SCONS:-scons}"

if [ ! -d "$RTT_ROOT" ]; then
    echo "RT-Thread source not found: $RTT_ROOT" >&2
    echo "Run 'bash setup.sh' first, or set RTT_ROOT." >&2
    exit 1
fi

export RTT_ROOT
export RTT_EXEC_PATH

cd "$HERE/bsp/stm32/stm32f103-kylin-v31"
"$SCONS" "$@"
arm-none-eabi-objcopy -O ihex rt-thread.elf rt-thread.hex
echo "Built: $PWD/rt-thread.elf"
echo "       $PWD/rt-thread.hex"

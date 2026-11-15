#!/usr/bin/env bash
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
JOBS="${JOBS:-4}"
make -C "$HERE" -j"$JOBS"

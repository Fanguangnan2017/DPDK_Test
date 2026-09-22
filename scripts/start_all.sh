#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-$(cd "$(dirname "$0")/../build" && pwd)}"

"$BUILD_DIR/rx_primary" \
    -l 2-17 \
    -n 4 \
    --file-prefix=industrial_rx \
    -- \
    --pci0=0000:18:00.0 \
    --pci1=0000:18:00.1 \
    > /var/log/industrial_rx_primary.log 2>&1 &

sleep 3

for ch in $(seq 0 11); do
    core=$((18 + ch))
    "$BUILD_DIR/channel_worker" \
        -l "$core" \
        -n 4 \
        --proc-type=secondary \
        --file-prefix=industrial_rx \
        -- \
        "$ch" \
        > "/var/log/industrial_rx_worker_${ch}.log" 2>&1 &
done

wait

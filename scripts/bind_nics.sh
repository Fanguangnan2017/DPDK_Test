#!/usr/bin/env bash
set -euo pipefail

# Replace with your real PCI ids
if command -v dpdk-devbind.py >/dev/null 2>&1; then
    dpdk-devbind.py -b vfio-pci 0000:18:00.0
    dpdk-devbind.py -b vfio-pci 0000:18:00.1
else
    echo "dpdk-devbind.py not found; skipping NIC binding"
fi

echo "NICs bound to vfio-pci"

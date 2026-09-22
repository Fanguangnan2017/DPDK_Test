#!/usr/bin/env bash
set -euo pipefail

echo 4096 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

mkdir -p /mnt/huge
mountpoint -q /mnt/huge || mount -t hugetlbfs none /mnt/huge

echo "Hugepages ready"

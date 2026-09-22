# Industrial DPDK RX

This project implements a DPDK-based receive framework with:
- 2 NICs
- 12 channels
- original Ethernet frame handling
- custom industrial protocol header
- separate LVDS / result / tx rings

## Build

```bash
mkdir -p build
cd build
cmake -DDPDK_ROOT="$PWD/../thirdparty/UNIX/lib/DPDK" ..
make -j$(nproc)
```

## Setup

```bash
sudo ./scripts/setup_hugepages.sh
sudo ./scripts/bind_nics.sh
```

## Run

Primary:
```bash
./rx_primary
```

Workers:
```bash
./channel_worker 0
./channel_worker 1
...
./channel_worker 11
```

## Notes

- Replace PCI ids and driver binding with your actual NIC hardware
- Use the actual DPDK version matching your environment
- For a true industrial deployment, add flow control / ACK / timeout / WAL checkpoint logic

#pragma once

#include <cstdint>
#include <string>

namespace industrial {

struct PortConfig {
    std::string pci0;
    std::string pci1;

    uint16_t rx_queues = 6;
    uint16_t tx_queues = 6;
    uint16_t burst_size = 64;
    uint32_t max_rx_pkt_len = 9728;
};

struct ReceiverConfig {
    PortConfig port_cfg;

    std::string wal_dir = "/data/industrial_rx/wal";
    bool enable_wal = true;
    bool enable_sequence_check = true;
    bool enable_flow_control = true;

    uint32_t ring_size = 16384;
    bool use_secondary_worker = true;
};

} // namespace industrial

#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include <rte_ring.h>

#include "protocol.hpp"

namespace industrial {

enum class QueueType : uint8_t {
    Lvds = 0,
    ControlResult = 1,
    ControlTx = 2,
    Count = 3
};

struct ChannelStats {
    std::atomic<uint64_t> lvds_frames{0};
    std::atomic<uint64_t> lvds_bytes{0};
    std::atomic<uint64_t> control_result_frames{0};
    std::atomic<uint64_t> control_tx_frames{0};
    std::atomic<uint64_t> dropped_frames{0};
    std::atomic<uint64_t> sequence_lost{0};
    std::atomic<uint64_t> crc_errors{0};
    std::atomic<uint64_t> queue_depth{0};
    std::atomic<uint64_t> last_sequence{0};
};

struct ChannelStatsSnapshot {
    uint64_t lvds_frames = 0;
    uint64_t lvds_bytes = 0;
    uint64_t control_result_frames = 0;
    uint64_t control_tx_frames = 0;
    uint64_t dropped_frames = 0;
    uint64_t sequence_lost = 0;
    uint64_t crc_errors = 0;
    uint64_t queue_depth = 0;
    uint64_t last_sequence = 0;
};

struct RuntimeStats {
    std::array<ChannelStats, kChannelCount> channels{};

    std::atomic<uint64_t> total_rx_frames{0};
    std::atomic<uint64_t> total_rx_bytes{0};
    std::atomic<uint64_t> total_dropped{0};
    std::atomic<uint64_t> total_crc_errors{0};
    std::atomic<uint64_t> wal_write_failures{0};
};

struct RuntimeStatsSnapshot {
    std::array<ChannelStatsSnapshot, kChannelCount> channels{};

    uint64_t total_rx_frames = 0;
    uint64_t total_rx_bytes = 0;
    uint64_t total_dropped = 0;
    uint64_t total_crc_errors = 0;
    uint64_t wal_write_failures = 0;
};

struct ChannelRings {
    rte_ring* lvds_rx = nullptr;
    rte_ring* control_result_rx = nullptr;
    rte_ring* control_tx = nullptr;
};

} // namespace industrial

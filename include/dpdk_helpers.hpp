#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

struct rte_mempool;

namespace industrial {

rte_mempool* create_packet_pool(unsigned socket_id);

bool configure_port(
    uint16_t port_id,
    uint16_t rx_queues,
    uint16_t tx_queues,
    uint16_t rx_desc_count,
    uint16_t tx_desc_count,
    rte_mempool* mempool);

bool write_wal_record(
    const std::string& wal_dir,
    uint16_t channel_id,
    const void* data,
    std::size_t data_len);

} // namespace industrial

#include "dpdk_helpers.hpp"

#include <rte_errno.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>

#include <iostream>

namespace industrial {

rte_mempool* create_packet_pool(unsigned socket_id) {
    constexpr unsigned mbuf_count = 262144;
    constexpr unsigned cache_size = 512;
    constexpr unsigned data_room_size = 9728 + RTE_PKTMBUF_HEADROOM;

    auto* pool = rte_pktmbuf_pool_create(
        "industrial_rx_mbuf_pool",
        mbuf_count,
        cache_size,
        0,
        data_room_size,
        socket_id);

    if (pool == nullptr) {
        std::cerr << "rte_pktmbuf_pool_create failed: "
                  << rte_strerror(rte_errno) << '\n';
        return nullptr;
    }

    return pool;
}

} // namespace industrial

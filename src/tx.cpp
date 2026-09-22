#include "industrial_rx.hpp"

#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_ring.h>

#include <cstring>
#include <iostream>
#include <string>

namespace industrial {

bool enqueue_tx_packet_from_ring(
    uint16_t port_id,
    uint16_t queue_id,
    const std::string& ring_name) {
    auto* ring = rte_ring_lookup(ring_name.c_str());
    if (ring == nullptr) {
        return false;
    }

    void* items[64];
    const unsigned count = rte_ring_dequeue_burst(
        ring,
        items,
        64,
        nullptr);

    if (count == 0) {
        return true;
    }

    rte_mbuf* pkts[64];
    for (unsigned i = 0; i < count; ++i) {
        pkts[i] = static_cast<rte_mbuf*>(items[i]);
    }

    const uint16_t sent = rte_eth_tx_burst(
        port_id,
        queue_id,
        pkts,
        static_cast<uint16_t>(count));

    for (uint16_t i = sent; i < count; ++i) {
        rte_pktmbuf_free(pkts[i]);
    }

    return true;
}

bool send_raw_control_frame(
    uint16_t port_id,
    uint16_t queue_id,
    const void* frame,
    std::size_t frame_len) {
    if (frame == nullptr || frame_len == 0) {
        return false;
    }

    auto* pool = rte_mempool_lookup("industrial_rx_mbuf_pool");
    if (pool == nullptr) {
        std::cerr << "tx pool not found\n";
        return false;
    }

    rte_mbuf* mbuf = rte_pktmbuf_alloc(pool);
    if (mbuf == nullptr) {
        return false;
    }

    if (frame_len > rte_pktmbuf_tailroom(mbuf)) {
        rte_pktmbuf_free(mbuf);
        return false;
    }

    void* dst = rte_pktmbuf_append(mbuf, frame_len);
    if (dst == nullptr) {
        rte_pktmbuf_free(mbuf);
        return false;
    }

    std::memcpy(dst, frame, frame_len);

    rte_mbuf* pkts[1];
    pkts[0] = mbuf;

    const uint16_t sent = rte_eth_tx_burst(
        port_id,
        queue_id,
        pkts,
        1);

    if (sent != 1) {
        rte_pktmbuf_free(mbuf);
        return false;
    }

    return true;
}

bool queue_raw_control_packet(
    const std::string& ring_name,
    const void* payload,
    std::size_t payload_len) {
    auto* ring = rte_ring_lookup(ring_name.c_str());
    if (ring == nullptr) {
        return false;
    }

    auto* pool = rte_mempool_lookup("industrial_rx_mbuf_pool");
    if (pool == nullptr) {
        return false;
    }

    rte_mbuf* mbuf = rte_pktmbuf_alloc(pool);
    if (mbuf == nullptr) {
        return false;
    }

    if (payload_len > rte_pktmbuf_tailroom(mbuf)) {
        rte_pktmbuf_free(mbuf);
        return false;
    }

    void* dst = rte_pktmbuf_append(mbuf, payload_len);
    if (dst == nullptr) {
        rte_pktmbuf_free(mbuf);
        return false;
    }

    std::memcpy(dst, payload, payload_len);

    if (rte_ring_enqueue(ring, mbuf) != 0) {
        rte_pktmbuf_free(mbuf);
        return false;
    }

    return true;
}

} // namespace industrial

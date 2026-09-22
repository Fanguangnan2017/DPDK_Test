#include "worker.hpp"

#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_ring.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace industrial {

namespace {

constexpr const char* kMbufPoolName = "industrial_rx_mbuf_pool";

bool dequeue_packet(
    rte_ring* ring,
    std::vector<uint8_t>& output,
    std::chrono::milliseconds timeout) {
    if (ring == nullptr) {
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        void* item = nullptr;

        if (rte_ring_dequeue(ring, &item) == 0) {
            auto* mbuf = static_cast<rte_mbuf*>(item);
            if (mbuf == nullptr) {
                continue;
            }

            const uint32_t packet_len = rte_pktmbuf_pkt_len(mbuf);
            const uint8_t* data = rte_pktmbuf_mtod(mbuf, const uint8_t*);

            if (data == nullptr || packet_len == 0) {
                rte_pktmbuf_free(mbuf);
                return false;
            }

            output.assign(data, data + packet_len);
            rte_pktmbuf_free(mbuf);
            return true;
        }

        std::this_thread::sleep_for(std::chrono::microseconds(20));
    }

    return false;
}

} // namespace

class ChannelWorker::Impl {
public:
    uint16_t channel_id = 0;
    rte_ring* lvds_ring = nullptr;
    rte_ring* control_result_ring = nullptr;
    rte_ring* control_tx_ring = nullptr;
    bool attached_ = false;

    bool attach(uint16_t ch,
                const std::string& lvds_ring_name,
                const std::string& result_ring_name,
                const std::string& tx_ring_name) {
        if (ch >= 12) {
            std::cerr << "invalid channel id: " << ch << '\n';
            return false;
        }

        channel_id = ch;

        lvds_ring = rte_ring_lookup(lvds_ring_name.c_str());
        control_result_ring = rte_ring_lookup(result_ring_name.c_str());
        control_tx_ring = rte_ring_lookup(tx_ring_name.c_str());

        if (!lvds_ring || !control_result_ring || !control_tx_ring) {
            std::cerr << "failed to attach worker rings for channel " << ch << '\n';
            detach();
            return false;
        }

        attached_ = true;
        return true;
    }

    bool dequeue_lvds(std::vector<uint8_t>& out,
                      std::chrono::milliseconds timeout) {
        if (!attached_ || !lvds_ring) {
            return false;
        }

        return dequeue_packet(lvds_ring, out, timeout);
    }

    bool dequeue_control_result(std::vector<uint8_t>& out,
                                std::chrono::milliseconds timeout) {
        if (!attached_ || !control_result_ring) {
            return false;
        }

        return dequeue_packet(control_result_ring, out, timeout);
    }

    bool enqueue_control_tx(const void* payload,
                            std::size_t payload_len,
                            uint32_t command_id) {
        if (!attached_ ||
            control_tx_ring == nullptr ||
            payload == nullptr ||
            payload_len == 0) {
            return false;
        }

        rte_mempool* pool = rte_mempool_lookup(kMbufPoolName);
        if (pool == nullptr) {
            std::cerr << "mempool not found: " << kMbufPoolName << '\n';
            return false;
        }

        rte_mbuf* mbuf = rte_pktmbuf_alloc(pool);
        if (mbuf == nullptr) {
            std::cerr << "rte_pktmbuf_alloc failed\n";
            return false;
        }

        if (payload_len > rte_pktmbuf_tailroom(mbuf)) {
            std::cerr << "payload too large for mbuf tailroom\n";
            rte_pktmbuf_free(mbuf);
            return false;
        }

        void* dst = rte_pktmbuf_append(mbuf, payload_len);
        if (dst == nullptr) {
            std::cerr << "rte_pktmbuf_append failed\n";
            rte_pktmbuf_free(mbuf);
            return false;
        }

        std::memcpy(dst, payload, payload_len);

        if (rte_ring_enqueue(control_tx_ring, mbuf) != 0) {
            std::cerr << "control tx ring is full, command_id=" << command_id << '\n';
            rte_pktmbuf_free(mbuf);
            return false;
        }

        return true;
    }

    uint16_t get_channel_id() const noexcept {
        return channel_id;
    }

    bool attached() const noexcept {
        return attached_;
    }

    void detach() noexcept {
        lvds_ring = nullptr;
        control_result_ring = nullptr;
        control_tx_ring = nullptr;
        attached_ = false;
    }
};

ChannelWorker::ChannelWorker()
    : impl_(std::make_unique<ChannelWorker::Impl>()) {}

ChannelWorker::~ChannelWorker() = default;

ChannelWorker::ChannelWorker(ChannelWorker&& other) noexcept
    : impl_(std::move(other.impl_)) {}

ChannelWorker& ChannelWorker::operator=(ChannelWorker&& other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);
    }
    return *this;
}

bool ChannelWorker::attach(
    uint16_t channel_id,
    const std::string& ring_name_lvds,
    const std::string& ring_name_control_result,
    const std::string& ring_name_control_tx) {
    if (!impl_) {
        impl_ = std::make_unique<ChannelWorker::Impl>();
    }

    return impl_->attach(
        channel_id,
        ring_name_lvds,
        ring_name_control_result,
        ring_name_control_tx);
}

bool ChannelWorker::dequeue_lvds(
    std::vector<uint8_t>& out,
    std::chrono::milliseconds timeout) {
    if (!impl_) {
        return false;
    }

    return impl_->dequeue_lvds(out, timeout);
}

bool ChannelWorker::dequeue_control_result(
    std::vector<uint8_t>& out,
    std::chrono::milliseconds timeout) {
    if (!impl_) {
        return false;
    }

    return impl_->dequeue_control_result(out, timeout);
}

bool ChannelWorker::enqueue_control_tx(
    const void* payload,
    std::size_t payload_len,
    uint32_t command_id) {
    if (!impl_) {
        return false;
    }

    return impl_->enqueue_control_tx(payload, payload_len, command_id);
}

uint16_t ChannelWorker::channel_id() const noexcept {
    return impl_ ? impl_->get_channel_id() : 0;
}

bool ChannelWorker::attached() const noexcept {
    return impl_ ? impl_->attached() : false;
}

void ChannelWorker::detach() noexcept {
    if (impl_) {
        impl_->detach();
    }
}

} // namespace industrial

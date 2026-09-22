#include "industrial_rx.hpp"
#include "dpdk_helpers.hpp"
#include "protocol.hpp"

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_ring.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

namespace industrial {

namespace {

std::string ring_name_for_channel(uint16_t channel_id, QueueType qtype) {
    const char* suffix = "lvds";
    switch (qtype) {
        case QueueType::Lvds:
            suffix = "lvds";
            break;
        case QueueType::ControlResult:
            suffix = "result";
            break;
        case QueueType::ControlTx:
            suffix = "tx";
            break;
        default:
            suffix = "lvds";
            break;
    }
    return "channel_" + std::to_string(channel_id) + "_" + suffix;
}

} // namespace

class PrimaryReceiver::Impl {
public:
    Impl() = default;

    ~Impl() {
        stop();
    }

    bool initialize(const ReceiverConfig& cfg) {
        config_ = cfg;

        static bool eal_done = false;
        if (!eal_done) {
            int argc = 1;
            char* argv[] = { const_cast<char*>("rx_primary"), nullptr };
            int ret = rte_eal_init(argc, argv);
            if (ret < 0) {
                std::cerr << "rte_eal_init failed\n";
                return false;
            }
            eal_done = true;
        }

        mbuf_pool_ = create_packet_pool(rte_socket_id());
        if (!mbuf_pool_) {
            return false;
        }

        for (uint16_t ch = 0; ch < kChannelCount; ++ch) {
            const std::string lvds_name = ring_name_for_channel(ch, QueueType::Lvds);
            const std::string result_name = ring_name_for_channel(ch, QueueType::ControlResult);
            const std::string tx_name = ring_name_for_channel(ch, QueueType::ControlTx);

            channels_[ch].lvds_rx = rte_ring_create(
                lvds_name.c_str(),
                config_.ring_size,
                SOCKET_ID_ANY,
                RING_F_EXACT_SZ);

            channels_[ch].control_result_rx = rte_ring_create(
                result_name.c_str(),
                config_.ring_size,
                SOCKET_ID_ANY,
                RING_F_EXACT_SZ);

            channels_[ch].control_tx = rte_ring_create(
                tx_name.c_str(),
                config_.ring_size,
                SOCKET_ID_ANY,
                RING_F_EXACT_SZ);

            if (!channels_[ch].lvds_rx ||
                !channels_[ch].control_result_rx ||
                !channels_[ch].control_tx) {
                std::cerr << "failed to create rings for channel " << ch << '\n';
                return false;
            }
        }

        if (!configure_port(0,
                            config_.port_cfg.rx_queues,
                            config_.port_cfg.tx_queues,
                            2048,
                            2048,
                            mbuf_pool_)) {
            std::cerr << "configure_port 0 failed\n";
            return false;
        }

        if (!configure_port(1,
                            config_.port_cfg.rx_queues,
                            config_.port_cfg.tx_queues,
                            2048,
                            2048,
                            mbuf_pool_)) {
            std::cerr << "configure_port 1 failed\n";
            return false;
        }

        std::filesystem::create_directories(config_.wal_dir);
        return true;
    }

    bool start() {
        if (running_.load()) {
            return true;
        }

        running_.store(true);
        rx_thread_ = std::thread(&PrimaryReceiver::Impl::rx_loop, this);
        tx_thread_ = std::thread(&PrimaryReceiver::Impl::tx_loop, this);
        return true;
    }

    void stop() {
        running_.store(false);

        if (rx_thread_.joinable()) {
            rx_thread_.join();
        }
        if (tx_thread_.joinable()) {
            tx_thread_.join();
        }
    }

    bool is_running() const noexcept {
        return running_.load();
    }

    RuntimeStatsSnapshot get_runtime_stats() const {
        RuntimeStatsSnapshot snapshot{};

        for (std::size_t i = 0; i < kChannelCount; ++i) {
            const auto& src = stats_.channels[i];
            auto& dst = snapshot.channels[i];

            dst.lvds_frames = src.lvds_frames.load(std::memory_order_relaxed);
            dst.lvds_bytes = src.lvds_bytes.load(std::memory_order_relaxed);
            dst.control_result_frames = src.control_result_frames.load(std::memory_order_relaxed);
            dst.control_tx_frames = src.control_tx_frames.load(std::memory_order_relaxed);
            dst.dropped_frames = src.dropped_frames.load(std::memory_order_relaxed);
            dst.sequence_lost = src.sequence_lost.load(std::memory_order_relaxed);
            dst.crc_errors = src.crc_errors.load(std::memory_order_relaxed);
            dst.queue_depth = src.queue_depth.load(std::memory_order_relaxed);
            dst.last_sequence = src.last_sequence.load(std::memory_order_relaxed);
        }

        snapshot.total_rx_frames = stats_.total_rx_frames.load(std::memory_order_relaxed);
        snapshot.total_rx_bytes = stats_.total_rx_bytes.load(std::memory_order_relaxed);
        snapshot.total_dropped = stats_.total_dropped.load(std::memory_order_relaxed);
        snapshot.total_crc_errors = stats_.total_crc_errors.load(std::memory_order_relaxed);
        snapshot.wal_write_failures = stats_.wal_write_failures.load(std::memory_order_relaxed);

        return snapshot;
    }

    ChannelStatsSnapshot get_channel_stats(uint16_t channel_id) const {
        ChannelStatsSnapshot snapshot{};
        if (channel_id >= kChannelCount) {
            return snapshot;
        }

        const auto& src = stats_.channels[channel_id];

        snapshot.lvds_frames = src.lvds_frames.load(std::memory_order_relaxed);
        snapshot.lvds_bytes = src.lvds_bytes.load(std::memory_order_relaxed);
        snapshot.control_result_frames = src.control_result_frames.load(std::memory_order_relaxed);
        snapshot.control_tx_frames = src.control_tx_frames.load(std::memory_order_relaxed);
        snapshot.dropped_frames = src.dropped_frames.load(std::memory_order_relaxed);
        snapshot.sequence_lost = src.sequence_lost.load(std::memory_order_relaxed);
        snapshot.crc_errors = src.crc_errors.load(std::memory_order_relaxed);
        snapshot.queue_depth = src.queue_depth.load(std::memory_order_relaxed);
        snapshot.last_sequence = src.last_sequence.load(std::memory_order_relaxed);

        return snapshot;
    }

    bool send_control_command(uint16_t channel_id,
                              uint32_t command_id,
                              const void* payload,
                              std::size_t payload_len) {
        if (channel_id >= kChannelCount ||
            payload == nullptr ||
            payload_len == 0) {
            return false;
        }

        auto* ring = channels_[channel_id].control_tx;
        if (!ring) {
            return false;
        }

        auto* mbuf = rte_pktmbuf_alloc(mbuf_pool_);
        if (!mbuf) {
            return false;
        }

        if (payload_len > rte_pktmbuf_tailroom(mbuf)) {
            rte_pktmbuf_free(mbuf);
            return false;
        }

        void* dst = rte_pktmbuf_append(mbuf, payload_len);
        if (!dst) {
            rte_pktmbuf_free(mbuf);
            return false;
        }

        std::memcpy(dst, payload, payload_len);

        if (rte_ring_enqueue(ring, mbuf) != 0) {
            rte_pktmbuf_free(mbuf);
            return false;
        }

        (void)command_id;
        return true;
    }

private:
    void rx_loop() {
        std::vector<rte_mbuf*> packets;
        packets.resize(64);

        while (running_.load()) {
            for (uint16_t port = 0; port < 2; ++port) {
                for (uint16_t q = 0; q < config_.port_cfg.rx_queues; ++q) {
                    const uint16_t nb = rte_eth_rx_burst(
                        port,
                        q,
                        packets.data(),
                        static_cast<uint16_t>(packets.size()));

                    for (uint16_t i = 0; i < nb; ++i) {
                        auto* mbuf = packets[i];
                        if (!handle_rx_packet(port, q, mbuf)) {
                            rte_pktmbuf_free(mbuf);
                        }
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::microseconds(20));
        }
    }

    void tx_loop() {
        std::vector<rte_mbuf*> packets;
        packets.resize(64);

        while (running_.load()) {
            for (uint16_t ch = 0; ch < kChannelCount; ++ch) {
                auto* ring = channels_[ch].control_tx;
                if (!ring) {
                    continue;
                }

                void* items[64];
                const unsigned n = rte_ring_dequeue_burst(
                    ring,
                    items,
                    64,
                    nullptr);

                if (n == 0) {
                    continue;
                }

                for (unsigned i = 0; i < n; ++i) {
                    packets[i] = static_cast<rte_mbuf*>(items[i]);
                }

                const uint16_t port = (ch < 6) ? 0 : 1;
                const uint16_t queue = (ch < 6) ? ch : (ch - 6);

                const uint16_t sent = rte_eth_tx_burst(
                    port,
                    queue,
                    packets.data(),
                    static_cast<uint16_t>(n));

                for (uint16_t i = sent; i < n; ++i) {
                    rte_pktmbuf_free(packets[i]);
                }

                stats_.channels[ch].control_tx_frames.fetch_add(
                    n,
                    std::memory_order_relaxed);
            }

            std::this_thread::sleep_for(std::chrono::microseconds(20));
        }
    }

    bool handle_rx_packet(uint16_t port, uint16_t queue, rte_mbuf* mbuf) {
        auto* raw = rte_pktmbuf_mtod(mbuf, uint8_t*);
        const std::size_t total_len = mbuf->data_len;

        if (total_len < 14 + sizeof(IndustrialHeader)) {
            return false;
        }

        ParsedFrame parsed{};
        if (!parse_frame(raw, total_len, parsed)) {
            return false;
        }

        const uint16_t expected_channel = port * kChannelsPerPort + queue;
        if (parsed.channel_id != expected_channel) {
            stats_.channels[parsed.channel_id].dropped_frames.fetch_add(
                1,
                std::memory_order_relaxed);
            return false;
        }

        const uint16_t ch = parsed.channel_id;
        const auto type = parsed.message_type;

        stats_.total_rx_frames.fetch_add(1, std::memory_order_relaxed);
        stats_.total_rx_bytes.fetch_add(total_len, std::memory_order_relaxed);

        if (config_.enable_wal) {
            if (!write_wal_record(config_.wal_dir, ch, raw, total_len)) {
                stats_.wal_write_failures.fetch_add(1, std::memory_order_relaxed);
            }
        }

        switch (type) {
            case MessageType::LvdsData: {
                auto* ring = channels_[ch].lvds_rx;
                if (!ring) return false;

                if (rte_ring_enqueue(ring, mbuf) != 0) {
                    stats_.total_dropped.fetch_add(1, std::memory_order_relaxed);
                    stats_.channels[ch].dropped_frames.fetch_add(1, std::memory_order_relaxed);
                    return false;
                }

                stats_.channels[ch].lvds_frames.fetch_add(1, std::memory_order_relaxed);
                stats_.channels[ch].lvds_bytes.fetch_add(parsed.payload_length, std::memory_order_relaxed);
                break;
            }

            case MessageType::ControlResult:
            case MessageType::StatusReport:
            case MessageType::Ack:
            case MessageType::Nack:
            case MessageType::ErrorReport: {
                auto* ring = channels_[ch].control_result_rx;
                if (!ring) return false;

                if (rte_ring_enqueue(ring, mbuf) != 0) {
                    stats_.total_dropped.fetch_add(1, std::memory_order_relaxed);
                    stats_.channels[ch].dropped_frames.fetch_add(1, std::memory_order_relaxed);
                    return false;
                }

                stats_.channels[ch].control_result_frames.fetch_add(1, std::memory_order_relaxed);
                break;
            }

            case MessageType::ControlCommand:
            case MessageType::FlowControl:
            case MessageType::Heartbeat: {
                stats_.total_dropped.fetch_add(1, std::memory_order_relaxed);
                stats_.channels[ch].dropped_frames.fetch_add(1, std::memory_order_relaxed);
                rte_pktmbuf_free(mbuf);
                return false;
            }

            default:
                rte_pktmbuf_free(mbuf);
                return false;
        }

        return true;
    }

private:
    ReceiverConfig config_{};
    std::array<ChannelRings, kChannelCount> channels_{};
    RuntimeStats stats_{};

    rte_mempool* mbuf_pool_ = nullptr;
    std::thread rx_thread_;
    std::thread tx_thread_;
    std::atomic_bool running_{false};
};

PrimaryReceiver::PrimaryReceiver()
    : impl_(std::make_unique<PrimaryReceiver::Impl>()) {}

PrimaryReceiver::~PrimaryReceiver() = default;

PrimaryReceiver::PrimaryReceiver(PrimaryReceiver&&) noexcept = default;
PrimaryReceiver& PrimaryReceiver::operator=(PrimaryReceiver&&) noexcept = default;

bool PrimaryReceiver::initialize(const ReceiverConfig& config) {
    return impl_->initialize(config);
}

bool PrimaryReceiver::start() {
    return impl_->start();
}

void PrimaryReceiver::stop() {
    impl_->stop();
}

bool PrimaryReceiver::is_running() const noexcept {
    return impl_->is_running();
}

RuntimeStatsSnapshot PrimaryReceiver::get_runtime_stats() const {
    return impl_->get_runtime_stats();
}

ChannelStatsSnapshot PrimaryReceiver::get_channel_stats(uint16_t channel_id) const {
    return impl_->get_channel_stats(channel_id);
}

bool PrimaryReceiver::send_control_command(
    uint16_t channel_id,
    uint32_t command_id,
    const void* payload,
    std::size_t payload_len) {
    return impl_->send_control_command(channel_id, command_id, payload, payload_len);
}

} // namespace industrial

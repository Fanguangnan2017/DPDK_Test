#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "protocol.hpp"
#include "receiver_config.hpp"
#include "shared_channel.hpp"
#include "worker.hpp"

namespace industrial {

class PrimaryReceiver {
public:
    PrimaryReceiver();
    ~PrimaryReceiver();

    PrimaryReceiver(const PrimaryReceiver&) = delete;
    PrimaryReceiver& operator=(const PrimaryReceiver&) = delete;

    PrimaryReceiver(PrimaryReceiver&&) noexcept;
    PrimaryReceiver& operator=(PrimaryReceiver&&) noexcept;

    bool initialize(const ReceiverConfig& config);
    bool start();
    void stop();

    bool is_running() const noexcept;

    RuntimeStatsSnapshot get_runtime_stats() const;
    ChannelStatsSnapshot get_channel_stats(uint16_t channel_id) const;

    bool send_control_command(
        uint16_t channel_id,
        uint32_t command_id,
        const void* payload,
        std::size_t payload_len);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace industrial

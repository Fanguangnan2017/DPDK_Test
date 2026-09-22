#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace industrial {

class ChannelWorker {
public:
    ChannelWorker();
    ~ChannelWorker();

    ChannelWorker(const ChannelWorker&) = delete;
    ChannelWorker& operator=(const ChannelWorker&) = delete;

    ChannelWorker(ChannelWorker&&) noexcept;
    ChannelWorker& operator=(ChannelWorker&&) noexcept;

    bool attach(
        uint16_t channel_id,
        const std::string& ring_name_lvds,
        const std::string& ring_name_control_result,
        const std::string& ring_name_control_tx);

    bool dequeue_lvds(
        std::vector<uint8_t>& out,
        std::chrono::milliseconds timeout);

    bool dequeue_control_result(
        std::vector<uint8_t>& out,
        std::chrono::milliseconds timeout);

    bool enqueue_control_tx(
        const void* payload,
        std::size_t payload_len,
        uint32_t command_id);

    uint16_t channel_id() const noexcept;
    bool attached() const noexcept;
    void detach() noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace industrial

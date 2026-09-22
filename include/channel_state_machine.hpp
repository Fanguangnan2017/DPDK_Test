#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace industrial {

enum class ChannelState : uint8_t {
    Init = 0,
    Ready = 1,
    Running = 2,
    Paused = 3,
    Error = 4,
    Recovering = 5
};

enum class CommandState : uint8_t {
    Idle = 0,
    Sent = 1,
    Acked = 2,
    Nacked = 3,
    Timeout = 4,
    Failed = 5
};

struct ChannelCommand {
    uint32_t command_id = 0;
    uint16_t channel_id = 0;
    uint64_t issued_at_ns = 0;
    uint64_t deadline_ms = 1000;
    uint8_t message_type = 0;
    std::array<uint8_t, 256> payload{};
    uint16_t payload_len = 0;
    CommandState state = CommandState::Idle;
};

struct ChannelStatus {
    ChannelState state = ChannelState::Init;
    uint64_t last_lvds_sequence = 0;
    uint64_t last_result_sequence = 0;
    uint64_t last_command_id = 0;
    uint64_t dropped_frames = 0;
    uint64_t invalid_sequence = 0;
    uint64_t flow_pause_count = 0;
    bool flow_paused = false;
    uint64_t high_water = 2000000;
    uint64_t low_water = 1000000;
};

class ChannelStateMachine {
public:
    ChannelStateMachine();
    ~ChannelStateMachine();

    void initialize(uint16_t channel_id);

    void on_lvds_frame(uint64_t sequence,
                       uint64_t timestamp_ns,
                       uint32_t payload_len);

    void on_control_result(uint32_t command_id,
                           uint64_t sequence,
                           uint8_t code);

    void on_control_command(uint32_t command_id,
                            uint8_t msg_type,
                            const void* payload,
                            uint16_t payload_len);

    void on_timeout_check(std::chrono::steady_clock::time_point now);

    bool should_pause_for_high_water() const noexcept;
    bool should_resume_for_low_water() const noexcept;

    ChannelStatus snapshot() const;
    void reset();

    bool enqueue_control_command(uint32_t command_id,
                                 uint8_t msg_type,
                                 const void* payload,
                                 uint16_t payload_len);

    std::vector<ChannelCommand> pending_commands() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace industrial

#include "channel_state_machine.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace industrial {

class ChannelStateMachine::Impl {
public:
    uint16_t channel_id_ = 0;
    ChannelStatus status_{};
    std::mutex mutex_;

    uint64_t expected_sequence_ = 0;
    uint64_t last_sequence_ = 0;

    std::unordered_map<uint32_t, ChannelCommand> commands_;
    std::vector<ChannelCommand> history_;

    void initialize(uint16_t channel_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        channel_id_ = channel_id;
        status_ = ChannelStatus{};
        status_.state = ChannelState::Ready;
        status_.high_water = 2000000ULL;
        status_.low_water = 1000000ULL;
        expected_sequence_ = 0;
        last_sequence_ = 0;
        commands_.clear();
        history_.clear();
    }

    void on_lvds_frame(uint64_t sequence,
                       uint64_t timestamp_ns,
                       uint32_t payload_len) {
        (void)timestamp_ns;
        std::lock_guard<std::mutex> lock(mutex_);

        if (status_.state == ChannelState::Paused) {
            return;
        }

        if (expected_sequence_ == 0 && last_sequence_ == 0) {
            expected_sequence_ = sequence + 1;
            last_sequence_ = sequence;
            status_.last_lvds_sequence = sequence;
            return;
        }

        if (sequence > expected_sequence_) {
            const uint64_t gap = sequence - expected_sequence_;
            status_.invalid_sequence += gap;
            status_.dropped_frames += gap;
            expected_sequence_ = sequence + 1;
            last_sequence_ = sequence;
            status_.last_lvds_sequence = sequence;
            status_.state = ChannelState::Error;
            return;
        }

        if (sequence == expected_sequence_ - 1) {
            last_sequence_ = sequence;
            status_.last_lvds_sequence = sequence;
            expected_sequence_ = sequence + 1;
        } else if (sequence < expected_sequence_ - 1) {
            status_.invalid_sequence++;
            status_.dropped_frames++;
        }

        if (payload_len > status_.high_water) {
            status_.flow_paused = true;
            status_.flow_pause_count++;
            status_.state = ChannelState::Paused;
        }

        if (payload_len < status_.low_water && status_.flow_paused) {
            status_.flow_paused = false;
            status_.state = ChannelState::Running;
        }
    }

    void on_control_result(uint32_t command_id,
                           uint64_t sequence,
                           uint8_t code) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = commands_.find(command_id);
        if (it == commands_.end()) {
            return;
        }

        auto& cmd = it->second;
        cmd.state = (code == 0) ? CommandState::Acked : CommandState::Nacked;
        status_.last_result_sequence = sequence;

        if (code == 0) {
            status_.state = ChannelState::Running;
        } else {
            status_.state = ChannelState::Error;
        }
    }

    void on_control_command(uint32_t command_id,
                            uint8_t msg_type,
                            const void* payload,
                            uint16_t payload_len) {
        std::lock_guard<std::mutex> lock(mutex_);

        ChannelCommand cmd{};
        cmd.command_id = command_id;
        cmd.channel_id = channel_id_;
        cmd.issued_at_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        cmd.deadline_ms = 1000;
        cmd.message_type = msg_type;
        cmd.payload_len = payload_len;
        cmd.state = CommandState::Sent;

        if (payload_len > 0 && payload != nullptr) {
            const uint16_t copy_len = std::min<uint16_t>(payload_len, 256);
            std::memcpy(cmd.payload.data(), payload, copy_len);
        }

        commands_[command_id] = cmd;
        history_.push_back(cmd);
        status_.last_command_id = command_id;
        status_.state = ChannelState::Running;
    }

    void on_timeout_check(std::chrono::steady_clock::time_point now) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();

        for (auto& pair : commands_) {
            auto& cmd = pair.second;
            auto issued_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::nanoseconds(cmd.issued_at_ns)).count();

            if (cmd.state == CommandState::Sent &&
                (now_ms - issued_ms) > static_cast<long long>(cmd.deadline_ms)) {
                cmd.state = CommandState::Timeout;
                status_.state = ChannelState::Recovering;
            }
        }
    }

    bool should_pause_for_high_water() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return status_.flow_paused;
    }

    bool should_resume_for_low_water() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return !status_.flow_paused && status_.state == ChannelState::Recovering;
    }

    ChannelStatus snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return status_;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        status_ = ChannelStatus{};
        status_.state = ChannelState::Ready;
        commands_.clear();
        history_.clear();
        expected_sequence_ = 0;
        last_sequence_ = 0;
    }

    bool enqueue_control_command(uint32_t command_id,
                                 uint8_t msg_type,
                                 const void* payload,
                                 uint16_t payload_len) {
        std::lock_guard<std::mutex> lock(mutex_);

        ChannelCommand cmd{};
        cmd.command_id = command_id;
        cmd.channel_id = channel_id_;
        cmd.issued_at_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        cmd.deadline_ms = 1000;
        cmd.message_type = msg_type;
        cmd.payload_len = payload_len;
        cmd.state = CommandState::Sent;

        if (payload_len > 0 && payload != nullptr) {
            const uint16_t copy_len = std::min<uint16_t>(payload_len, 256);
            std::memcpy(cmd.payload.data(), payload, copy_len);
        }

        commands_[command_id] = cmd;
        history_.push_back(cmd);
        status_.last_command_id = command_id;
        status_.state = ChannelState::Running;

        return true;
    }

    std::vector<ChannelCommand> pending_commands() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<ChannelCommand> out;
        out.reserve(commands_.size());
        for (const auto& pair : commands_) {
            out.push_back(pair.second);
        }
        return out;
    }
};

ChannelStateMachine::ChannelStateMachine()
    : impl_(std::make_unique<Impl>()) {}

ChannelStateMachine::~ChannelStateMachine() = default;

void ChannelStateMachine::initialize(uint16_t channel_id) {
    impl_->initialize(channel_id);
}

void ChannelStateMachine::on_lvds_frame(uint64_t sequence,
                                       uint64_t timestamp_ns,
                                       uint32_t payload_len) {
    impl_->on_lvds_frame(sequence, timestamp_ns, payload_len);
}

void ChannelStateMachine::on_control_result(uint32_t command_id,
                                           uint64_t sequence,
                                           uint8_t code) {
    impl_->on_control_result(command_id, sequence, code);
}

void ChannelStateMachine::on_control_command(uint32_t command_id,
                                            uint8_t msg_type,
                                            const void* payload,
                                            uint16_t payload_len) {
    impl_->on_control_command(command_id, msg_type, payload, payload_len);
}

void ChannelStateMachine::on_timeout_check(std::chrono::steady_clock::time_point now) {
    impl_->on_timeout_check(now);
}

bool ChannelStateMachine::should_pause_for_high_water() const noexcept {
    return impl_->should_pause_for_high_water();
}

bool ChannelStateMachine::should_resume_for_low_water() const noexcept {
    return impl_->should_resume_for_low_water();
}

ChannelStatus ChannelStateMachine::snapshot() const {
    return impl_->snapshot();
}

void ChannelStateMachine::reset() {
    impl_->reset();
}

bool ChannelStateMachine::enqueue_control_command(uint32_t command_id,
                                                 uint8_t msg_type,
                                                 const void* payload,
                                                 uint16_t payload_len) {
    return impl_->enqueue_control_command(command_id, msg_type, payload, payload_len);
}

std::vector<ChannelCommand> ChannelStateMachine::pending_commands() const {
    return impl_->pending_commands();
}

} // namespace industrial

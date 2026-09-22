#include "worker.hpp"
#include "channel_state_machine.hpp"

#include <rte_eal.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

int main(int argc, char** argv) {
    const int eal_ret = rte_eal_init(argc, argv);
    if (eal_ret < 0) {
        std::cerr << "rte_eal_init failed" << std::endl;
        return 1;
    }

    int app_argc = argc - eal_ret;
    char** app_argv = argv + eal_ret;

    uint16_t channel_id = 0;
    if (app_argc >= 2) {
        channel_id = static_cast<uint16_t>(std::atoi(app_argv[1]));
    }

    if (channel_id >= 12) {
        std::cerr << "invalid channel id: " << channel_id << std::endl;
        return 2;
    }

    industrial::ChannelStateMachine state_machine;
    state_machine.initialize(channel_id);

    const std::string lvds_name = "channel_" + std::to_string(channel_id) + "_lvds";
    const std::string result_name = "channel_" + std::to_string(channel_id) + "_result";
    const std::string tx_name = "channel_" + std::to_string(channel_id) + "_tx";

    industrial::ChannelWorker worker;
    if (!worker.attach(channel_id, lvds_name, result_name, tx_name)) {
        std::cerr << "worker attach failed for channel " << channel_id << std::endl;
        return 3;
    }

    std::cout << "worker attached to channel " << channel_id << std::endl;

    while (worker.attached()) {
        std::vector<uint8_t> result;
        if (worker.dequeue_control_result(result, std::chrono::milliseconds(20))) {
            std::cout << "result payload size=" << result.size() << std::endl;
            // decode command_id + ack/nack + update state machine here
            // state_machine.on_control_result(...);
        }

        std::vector<uint8_t> lvds;
        if (worker.dequeue_lvds(lvds, std::chrono::milliseconds(10))) {
            std::cout << "lvds payload size=" << lvds.size() << std::endl;
            // decode sequence + payload_len + update state machine here
            // state_machine.on_lvds_frame(...);
        }

        state_machine.on_timeout_check(std::chrono::steady_clock::now());
        auto status = state_machine.snapshot();
        if (status.state == industrial::ChannelState::Paused) {
            std::cout << "channel paused" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return 0;
}

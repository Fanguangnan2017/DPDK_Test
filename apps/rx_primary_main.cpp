#include "industrial_rx.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

namespace {
std::atomic<bool> g_stop{false};

void signal_handler(int) {
    g_stop.store(true);
}
}

int main() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    industrial::ReceiverConfig cfg;
    cfg.port_cfg.pci0 = "0000:18:00.0";
    cfg.port_cfg.pci1 = "0000:18:00.1";
    cfg.port_cfg.rx_queues = 6;
    cfg.port_cfg.tx_queues = 6;
    cfg.port_cfg.burst_size = 64;
    cfg.port_cfg.max_rx_pkt_len = 9728;
    cfg.wal_dir = "/data/industrial_rx/wal";
    cfg.enable_wal = true;
    cfg.enable_sequence_check = true;
    cfg.enable_flow_control = true;

    industrial::PrimaryReceiver primary;

    if (!primary.initialize(cfg)) {
        std::cerr << "primary initialize failed" << std::endl;
        return 1;
    }

    if (!primary.start()) {
        std::cerr << "primary start failed" << std::endl;
        return 2;
    }

    std::cout << "primary started" << std::endl;

    while (!g_stop.load()) {
        const auto stats = primary.get_runtime_stats();

        std::cout << "rx=" << stats.total_rx_frames
                  << " dropped=" << stats.total_dropped
                  << " crc_err=" << stats.total_crc_errors
                  << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    primary.stop();
    std::cout << "primary stopped" << std::endl;
    return 0;
}

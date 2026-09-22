#include "dpdk_helpers.hpp"

#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>

#include <iostream>

namespace industrial {

bool configure_port(
    uint16_t port_id,
    uint16_t rx_queues,
    uint16_t tx_queues,
    uint16_t rx_desc_count,
    uint16_t tx_desc_count,
    rte_mempool* mempool) {
    if (!rte_eth_dev_is_valid_port(port_id)) {
        return false;
    }

    rte_eth_dev_info dev_info{};
    if (rte_eth_dev_info_get(port_id, &dev_info) != 0) {
        std::cerr << "rte_eth_dev_info_get failed for port " << port_id << '\n';
        return false;
    }

    rte_eth_conf port_conf{};
    port_conf.rxmode.mq_mode = ETH_MQ_RX_RSS;
    port_conf.rxmode.max_rx_pkt_len = 9728;
    port_conf.rx_adv_conf.rss_conf.rss_hf =
        ETH_RSS_IP | ETH_RSS_UDP | ETH_RSS_TCP;

    const int ret = rte_eth_dev_configure(
        port_id,
        rx_queues,
        tx_queues,
        &port_conf);

    if (ret < 0) {
        std::cerr << "rte_eth_dev_configure failed port="
                  << port_id << " ret=" << ret << '\n';
        return false;
    }

    for (uint16_t q = 0; q < rx_queues; ++q) {
        const int queue_ret = rte_eth_rx_queue_setup(
            port_id,
            q,
            rx_desc_count,
            rte_eth_dev_socket_id(port_id),
            nullptr,
            mempool);

        if (queue_ret < 0) {
            std::cerr << "rte_eth_rx_queue_setup failed port="
                      << port_id << " queue=" << q
                      << " ret=" << queue_ret << '\n';
            return false;
        }
    }

    for (uint16_t q = 0; q < tx_queues; ++q) {
        const int queue_ret = rte_eth_tx_queue_setup(
            port_id,
            q,
            tx_desc_count,
            rte_eth_dev_socket_id(port_id),
            nullptr);

        if (queue_ret < 0) {
            std::cerr << "rte_eth_tx_queue_setup failed port="
                      << port_id << " queue=" << q
                      << " ret=" << queue_ret << '\n';
            return false;
        }
    }

    if (rte_eth_dev_start(port_id) < 0) {
        std::cerr << "rte_eth_dev_start failed port=" << port_id << '\n';
        return false;
    }

    rte_eth_promiscuous_enable(port_id);
    return true;
}

} // namespace industrial

#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace flow_scope
{

    // 单个网卡的指标
    struct InterfaceMetrics
    {
        std::string name;
        double rtt_ms = 0.0;
        double packet_loss_rate = 0.0;
        uint64_t rx_bps = 0;
        uint64_t tx_bps = 0;
        // --- 新增字段 ---
        uint64_t tcp_retrans_total = 0; // 累计重传次数
    };

    // 系统快照
    struct SystemSnapshot
    {
        uint64_t timestamp;
        std::vector<InterfaceMetrics> interfaces;

        nlohmann::json to_json() const
        {
            nlohmann::json j;
            j["system"] = "flow_scope"; // 修改为 snake_case
            j["timestamp"] = timestamp;
            j["interfaces"] = nlohmann::json::array();
            for (const auto &iface : interfaces)
            {
                j["interfaces"].push_back({{"name", iface.name},
                                           {"rtt_ms", iface.rtt_ms},
                                           {"loss_rate", iface.packet_loss_rate},
                                           {"rx_bps", iface.rx_bps},
                                           {"tcp_retrans", iface.tcp_retrans_total}}); // 暴露给 JSON
            }
            return j;
        }
    };

} // namespace flow_scope
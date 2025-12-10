#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace flow_scope
{

    // 强制 64 字节对齐，避免多核 Cache 颠簸
    struct alignas(64) InterfaceMetrics
    {
        std::string name;
        double rtt_ms = 0.0;
        double packet_loss_rate = 0.0;
        uint64_t rx_bps = 0;
        uint64_t tx_bps = 0;
        uint64_t tcp_retrans_total = 0;
    };

    struct SystemSnapshot
    {
        uint64_t timestamp;
        std::vector<InterfaceMetrics> interfaces;

        // 为了复用内存，我们增加一个 reset 方法，而不是销毁对象
        void reset()
        {
            // timestamp 更新由调用者负责
            // interfaces 不 clear，而是保留 capacity，避免重新分配内存
            // 实际逻辑中，如果网卡数量不变，甚至不需要动 vector，这里简化处理
            interfaces.clear();
        }

        nlohmann::json to_json() const
        {
            // ... (保持原样)
            nlohmann::json j;
            j["system"] = "flow_scope";
            j["timestamp"] = timestamp;
            j["interfaces"] = nlohmann::json::array();
            for (const auto &iface : interfaces)
            {
                j["interfaces"].push_back({{"name", iface.name},
                                           {"rtt_ms", iface.rtt_ms},
                                           {"loss_rate", iface.packet_loss_rate},
                                           {"rx_bps", iface.rx_bps},
                                           {"tx_bps", iface.tx_bps},
                                           {"tcp_retrans", iface.tcp_retrans_total}});
            }
            return j;
        }
    };

} // namespace flow_scope
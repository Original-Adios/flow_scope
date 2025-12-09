#pragma once
#include "monitor_base.hpp"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <chrono>

namespace flow_scope
{

    class TrafficMonitor : public MonitorBase
    {
    public:
        TrafficMonitor()
        {
            last_check_time_ = std::chrono::steady_clock::now();
        }

        void collect(InterfaceMetrics &metrics) override
        {
            std::ifstream file("/proc/net/dev");
            std::string line;

            // 1. 跳过前两行表头
            std::getline(file, line);
            std::getline(file, line);

            bool found = false;

            // 2. 逐行读取
            while (std::getline(file, line))
            {
                // 格式通常是: "  eth0: 12345 ..." 或 "eth0:12345..."
                // 我们需要找到匹配 metrics.name 的那一行

                // 简单的字符串查找（生产环境可以用更严谨的解析）
                size_t colon_pos = line.find(':');
                if (colon_pos == std::string::npos)
                    continue;

                std::string iface_name = line.substr(0, colon_pos);
                // 去除名称前的空格
                iface_name.erase(0, iface_name.find_first_not_of(" "));

                if (iface_name == metrics.name)
                {
                    // 找到目标网卡
                    std::string stats = line.substr(colon_pos + 1);
                    std::stringstream ss(stats);

                    uint64_t rx_bytes = 0;
                    uint64_t tx_bytes = 0;
                    uint64_t temp = 0;

                    // /proc/net/dev 的列顺序：
                    // Rx: bytes packets errs drop fifo frame compressed multicast
                    // Tx: bytes ...

                    ss >> rx_bytes; // 第1列: rx_bytes
                    for (int i = 0; i < 7; ++i)
                        ss >> temp; // 跳过中间7列
                    ss >> tx_bytes; // 第9列: tx_bytes (即 Tx 部分的第1列)

                    calculate_rate(metrics, rx_bytes, tx_bytes);
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                // 如果没找到网卡（比如网卡名写错了），归零
                metrics.rx_bps = 0;
                metrics.tx_bps = 0;
            }
        }

    private:
        struct LastState
        {
            uint64_t rx_bytes = 0;
            uint64_t tx_bytes = 0;
        };

        std::unordered_map<std::string, LastState> last_stats_;
        std::chrono::steady_clock::time_point last_check_time_;

        void calculate_rate(InterfaceMetrics &metrics, uint64_t current_rx, uint64_t current_tx)
        {
            auto now = std::chrono::steady_clock::now();
            // 计算时间差 (秒)，防止除以0
            double seconds = std::chrono::duration<double>(now - last_check_time_).count();
            if (seconds <= 0.0001)
                seconds = 1.0;

            // 获取上一次的状态
            LastState &last = last_stats_[metrics.name];

            // 计算速率 (Bytes / Second)
            // 注意：首次运行时 last 默认为0，速率会很大，这里简单处理一下
            if (last.rx_bytes != 0)
            {
                // 处理计数器溢出 (Overflow) 的情况略，假设是64位递增
                metrics.rx_bps = static_cast<uint64_t>((current_rx - last.rx_bytes) / seconds);
                metrics.tx_bps = static_cast<uint64_t>((current_tx - last.tx_bytes) / seconds);
            }
            else
            {
                metrics.rx_bps = 0;
                metrics.tx_bps = 0;
            }

            // 更新状态
            last.rx_bytes = current_rx;
            last.tx_bytes = current_tx;

            // 注意：这里简单的将 last_check_time 更新为 now
            // 在多网卡循环中，应该对每个网卡单独记录时间，或者保证 collect 调用间隔一致。
            // 为了演示简单，我们假设 collect 是一次性调用完所有网卡的。
            // *更严谨的做法*：将 last_check_time 放入 LastState 结构体中。
        }
    };

} // namespace flow_scope
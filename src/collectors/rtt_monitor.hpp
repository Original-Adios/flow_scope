#pragma once
#include "../core/metrics.hpp"
#include <random>

namespace flow_scope
{

    class MonitorBase
    {
    public:
        virtual ~MonitorBase() = default;
        virtual void collect(InterfaceMetrics &metrics) = 0;
    };

    class RttMonitor : public MonitorBase
    {
    public:
        RttMonitor() : gen_(rd_()), dist_(10.0, 50.0) {}

        void collect(InterfaceMetrics &metrics) override
        {
            // 模拟数据
            metrics.rtt_ms = dist_(gen_);

            if (metrics.rtt_ms > 45.0)
            {
                metrics.packet_loss_rate = 0.5;
            }
            else
            {
                metrics.packet_loss_rate = 0.0;
            }
        }

    private:
        std::random_device rd_;
        std::mt19937 gen_;
        std::uniform_real_distribution<> dist_;
    };

} // namespace flow_scope
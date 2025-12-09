#pragma once
#include "../core/metrics.hpp"

namespace flow_scope
{
    class MonitorBase
    {
    public:
        virtual ~MonitorBase() = default;
        virtual void collect(InterfaceMetrics &metrics) = 0;
    };

} // namespace flow_scope
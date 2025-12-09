#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <unistd.h>
#include <fstream>
#include "core/manager.hpp"
#include "server/http_server.hpp"
#include "collectors/rtt_monitor.hpp"
#include "collectors/traffic_monitor.hpp"
#include "collectors/loss_monitor.hpp" // 新增

using namespace flow_scope;

void monitor_loop()
{
    // 实例化监控器
    RttMonitor rtt_mon("8.8.8.8");
    TrafficMonitor traffic_mon;
    LossMonitor loss_mon; // eBPF 监控器

    // 自动寻找网卡
    std::string target_iface = "lo";
    std::ifstream file("/proc/net/dev");
    std::string line;
    while (std::getline(file, line))
    {
        if (line.find("ens33") != std::string::npos)
        {
            target_iface = "ens33";
            break;
        }
        if (line.find("eth0") != std::string::npos)
        {
            target_iface = "eth0";
            break;
        }
        if (line.find("wlan0") != std::string::npos)
        {
            target_iface = "wlan0";
            break;
        }
    }
    std::cout << "Monitoring Interface: " << target_iface << std::endl;

    while (true)
    {
        SystemSnapshot snapshot;
        snapshot.timestamp = std::time(nullptr);

        InterfaceMetrics iface_data;
        iface_data.name = target_iface;

        // 并行采集 (逻辑上是串行的，但操作非常快)
        rtt_mon.collect(iface_data);     // ICMP
        traffic_mon.collect(iface_data); // Procfs
        loss_mon.collect(iface_data);    // eBPF (读取 Map)

        snapshot.interfaces.push_back(iface_data);

        Manager::get_instance().update_snapshot(snapshot);

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main()
{
    if (geteuid() != 0)
    {
        std::cerr << "ERROR: flow_scope requires ROOT privileges for eBPF/RawSockets." << std::endl;
        return 1;
    }

    std::cout << "flow_scope agent v0.3.0 (eBPF Enabled)" << std::endl;

    std::thread collector(monitor_loop);
    collector.detach();

    HttpServer server(8080);
    server.start();

    return 0;
}
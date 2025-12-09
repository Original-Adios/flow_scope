#include <iostream>
#include <thread>
#include <chrono>
#include "core/manager.hpp"
#include "server/http_server.hpp"
#include "collectors/rtt_monitor.hpp"
#include "collectors/traffic_monitor.hpp"

using namespace flow_scope;

void monitor_loop()
{
    // 1. 真实 RTT 监控 (Ping 8.8.8.8)
    RttMonitor rtt_mon("1.2.3.4");

    // 2. 真实流量监控
    TrafficMonitor traffic_mon;

    // TODO: 记得改为你的网卡名 (如 eth0, ens33, wlo1)
    std::string target_iface = "ens33";

    while (true)
    {
        SystemSnapshot snapshot;
        snapshot.timestamp = std::time(nullptr);

        InterfaceMetrics iface_data;
        iface_data.name = target_iface;

        rtt_mon.collect(iface_data);     // 发送 Ping
        traffic_mon.collect(iface_data); // 读取流量

        snapshot.interfaces.push_back(iface_data);

        Manager::get_instance().update_snapshot(snapshot);

        // 采样间隔 1秒
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main()
{
    // 检查是否是 Root 用户，否则 RTT 会失败
    if (geteuid() != 0)
    {
        std::cerr << "WARNING: Not running as root. RTT monitoring will fail!" << std::endl;
        std::cerr << "Usage: sudo ./flow_scope" << std::endl;
    }

    std::cout << "flow_scope agent v0.2.2 (Real RTT + Traffic)" << std::endl;

    std::thread collector(monitor_loop);
    collector.detach();

    HttpServer server(8080);
    server.start();

    return 0;
}
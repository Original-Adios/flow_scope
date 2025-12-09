#include <iostream>
#include <thread>
#include <chrono>
#include "core/manager.hpp"
#include "server/http_server.hpp"
#include "collectors/rtt_monitor.hpp"

using namespace flow_scope;

// 模拟采集循环
void monitor_loop()
{
    RttMonitor rtt_mon;

    while (true)
    {
        SystemSnapshot snapshot;
        snapshot.timestamp = std::time(nullptr);

        InterfaceMetrics eth0;
        eth0.name = "eth0";
        rtt_mon.collect(eth0);

        snapshot.interfaces.push_back(eth0);

        Manager::get_instance().update_snapshot(snapshot);

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main()
{
    std::cout << "flow_scope agent v0.1.0 (Simulation Mode)" << std::endl;

    // 1. 启动后台采集
    std::thread collector(monitor_loop);
    collector.detach();

    // 2. 启动前台 HTTP 服务
    HttpServer server(8080);
    server.start();

    return 0;
}
#include <iostream>
#include <thread>
#include <vector>
#include <fstream>
#include "core/manager.hpp"
#include "core/scheduler.hpp" // 新增
#include "server/http_server.hpp"
#include "collectors/rtt_monitor.hpp"
#include "collectors/traffic_monitor.hpp"
#include "collectors/loss_monitor.hpp"

using namespace flow_scope;

int main()
{
    if (geteuid() != 0)
    {
        std::cerr << "ERROR: Root privileges required." << std::endl;
        return 1;
    }

    std::cout << "flow_scope agent v0.4.0 (Performance Optimized)" << std::endl;

    // 1. 初始化采集模块
    // 注意：我们将它们声明为 static 或者放在堆上，确保在 lambda 中有效
    // 为了简单，这里直接实例化在 main 栈上，引用捕获即可
    RttMonitor rtt_mon("8.8.8.8");
    TrafficMonitor traffic_mon;
    LossMonitor loss_mon;

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
    std::cout << "Target Interface: " << target_iface << std::endl;

    // 2. 初始化调度器
    Scheduler scheduler;

    // 3. 注册 1Hz (1000ms) 的采集任务
    scheduler.add_timer_task(1000, [&]()
                             {
        // --- 核心采集逻辑 (零分配版本) ---
        
        auto& mgr = Manager::get_instance();
        // 直接获取预分配好的内存，不 new
        auto* snapshot = mgr.get_background_buffer();
        
        // 更新时间戳
        snapshot->timestamp = std::time(nullptr);
        
        // 准备指标对象 (复用 vector 里的空间)
        InterfaceMetrics iface_data;
        iface_data.name = target_iface;

        // 采集
        rtt_mon.collect(iface_data);
        traffic_mon.collect(iface_data);
        loss_mon.collect(iface_data);
        
        // 放入快照
        snapshot->interfaces.push_back(iface_data);

        // 发布 (交换指针)
        mgr.publish_snapshot(); });

    // 4. 启动 HTTP 服务 (在单独线程)
    std::thread http_thread([]()
                            {
        HttpServer server(8080);
        server.start(); });
    http_thread.detach();

    // 5. 启动调度器 (主线程阻塞在此，处理 epoll 事件)
    scheduler.run();

    return 0;
}
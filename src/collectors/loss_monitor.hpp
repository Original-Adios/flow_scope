#pragma once
#include "monitor_base.hpp"
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <sys/resource.h>
#include <iostream>

// 包含自动生成的骨架头文件
#include "tcp_loss.skel.h"

namespace flow_scope
{

    class LossMonitor : public MonitorBase
    {
    public:
        LossMonitor()
        {
            // 1. 调整 RLIMIT_MEMLOCK
            // eBPF Map 需要锁定内存，默认限制通常太小，必须调大
            struct rlimit rlim = {RLIM_INFINITY, RLIM_INFINITY};
            if (setrlimit(RLIMIT_MEMLOCK, &rlim))
            {
                perror("setrlimit(RLIMIT_MEMLOCK) failed");
            }

            // 2. 打开 Skeleton (Open)
            skel_ = tcp_loss_bpf__open();
            if (!skel_)
            {
                std::cerr << "Failed to open BPF skeleton" << std::endl;
                return;
            }

            // 3. 加载并验证 (Load)
            if (tcp_loss_bpf__load(skel_))
            {
                std::cerr << "Failed to load BPF skeleton" << std::endl;
                tcp_loss_bpf__destroy(skel_);
                skel_ = nullptr;
                return;
            }

            // 4. 挂载到内核 Tracepoint (Attach)
            if (tcp_loss_bpf__attach(skel_))
            {
                std::cerr << "Failed to attach BPF skeleton" << std::endl;
                tcp_loss_bpf__destroy(skel_);
                skel_ = nullptr;
                return;
            }

            std::cout << "--> eBPF TCP Loss Monitor attached successfully!" << std::endl;
        }

        ~LossMonitor()
        {
            if (skel_)
            {
                tcp_loss_bpf__destroy(skel_);
            }
        }

        void collect(InterfaceMetrics &metrics) override
        {
            if (!skel_)
                return;

            // 5. 读取 Map 数据
            // 在 BPF 代码中，我们定义了 Key=0 存储计数
            uint32_t key = 0;
            uint64_t val = 0;

            // 获取 Map 的文件描述符 (通过 skeleton 直接获取，非常方便)
            int map_fd = bpf_map__fd(skel_->maps.tcp_retrans_counter);

            // 执行查找
            if (bpf_map_lookup_elem(map_fd, &key, &val) == 0)
            {
                metrics.tcp_retrans_total = val;
                std::cout << "[RETRANS] " << " Total=" << metrics.tcp_retrans_total << std::endl;
            }

            // 注意：目前的 eBPF 代码统计的是“全局”重传，无法区分具体是哪个网卡。
            // 所以我们把这个值赋给 metrics，表示系统级的状态。
        }

    private:
        struct tcp_loss_bpf *skel_ = nullptr;
    };

} // namespace flow_scope
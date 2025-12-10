#pragma once
#include <memory>
#include <mutex>
#include <atomic>
#include "metrics.hpp"

namespace flow_scope
{

    class Manager
    {
    public:
        static Manager &get_instance()
        {
            static Manager instance;
            return instance;
        }

        // --- 读者接口 (HTTP 线程) ---
        // 返回当前“前台”数据的只读引用，完全无锁
        std::shared_ptr<const SystemSnapshot> get_snapshot() const
        {
            // 原子读取 shared_ptr (C++11/17 方式)
            // load 操作非常快，几乎无开销
            return std::atomic_load(&active_snapshot_);
        }

        // --- 写者接口 (采集线程) ---
        // 获取“后台”缓冲区的引用，用于写入数据
        SystemSnapshot *get_background_buffer()
        {
            return background_buffer_.get();
        }

        // 提交数据：将后台切换为前台
        void publish_snapshot()
        {
            // 1. 准备要交换的指针
            std::shared_ptr<SystemSnapshot> new_active = background_buffer_;

            // 2. 原子交换 (Exchange)
            // active_snapshot_ 变成新的数据
            // old_active 拿到旧的数据 (即现在的后台)
            std::shared_ptr<SystemSnapshot> old_active = std::atomic_exchange(&active_snapshot_, new_active);

            // 3. 将旧的前台数据回收，作为下一次的后台缓冲区
            // 这样我们就实现了 0 内存分配 (Zero Allocation)
            background_buffer_ = old_active;

            // 4. 重置后台数据，准备下一次采集
            background_buffer_->reset();
        }

    private:
        Manager()
        {
            // 初始化两个缓冲区
            auto s1 = std::make_shared<SystemSnapshot>();
            auto s2 = std::make_shared<SystemSnapshot>();

            // 初始化原子指针
            std::atomic_store(&active_snapshot_, s1);
            background_buffer_ = s2;
        }

        // 前台指针 (读者看这个)
        std::shared_ptr<SystemSnapshot> active_snapshot_;

        // 后台指针 (写者写这个)，这是线程私有的，不需要原子保护
        std::shared_ptr<SystemSnapshot> background_buffer_;
    };

} // namespace flow_scope
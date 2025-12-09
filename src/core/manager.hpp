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

        // 读者接口
        std::shared_ptr<SystemSnapshot> get_snapshot()
        {
            std::lock_guard<std::mutex> lock(swap_mutex_);
            return active_snapshot_;
        }

        // 写者接口
        void update_snapshot(const SystemSnapshot &new_data)
        {
            auto new_ptr = std::make_shared<SystemSnapshot>(new_data);
            {
                std::lock_guard<std::mutex> lock(swap_mutex_);
                active_snapshot_ = new_ptr;
            }
        }

    private:
        Manager()
        {
            active_snapshot_ = std::make_shared<SystemSnapshot>();
        }

        std::mutex swap_mutex_;
        std::shared_ptr<SystemSnapshot> active_snapshot_;
    };

} // namespace flow_scope
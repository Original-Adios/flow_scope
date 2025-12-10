#pragma once
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <functional>
#include <iostream>
#include <vector>

namespace flow_scope
{

    class Scheduler
    {
    public:
        Scheduler()
        {
            // 1. 创建 epoll 实例
            epoll_fd_ = epoll_create1(0);
            if (epoll_fd_ < 0)
                perror("epoll_create1 failed");
        }

        ~Scheduler()
        {
            if (epoll_fd_ >= 0)
                close(epoll_fd_);
        }

        // 添加定时任务
        // interval_ms: 毫秒
        // callback: 触发时执行的函数
        void add_timer_task(int interval_ms, std::function<void()> callback)
        {
            int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

            // 设置定时时间
            struct timespec ts;
            ts.tv_sec = interval_ms / 1000;
            ts.tv_nsec = (interval_ms % 1000) * 1000000;

            struct itimerspec its;
            its.it_interval = ts; // 周期性触发
            its.it_value = ts;    // 首次触发时间

            timerfd_settime(tfd, 0, &its, nullptr);

            // 加入 epoll 监听
            struct epoll_event ev;
            ev.events = EPOLLIN;
            ev.data.fd = tfd;
            epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, tfd, &ev);

            // 保存 callback 和 fd 的映射，防止内存泄漏 (简单实现)
            tasks_.push_back({tfd, callback});
        }

        // 开始事件循环 (阻塞)
        void run()
        {
            const int MAX_EVENTS = 10;
            struct epoll_event events[MAX_EVENTS];

            std::cout << "Scheduler: Event loop started (epoll)." << std::endl;

            while (running_)
            {
                // 等待事件，无事件时 CPU 挂起 (Wait)
                int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, -1);

                for (int i = 0; i < nfds; ++i)
                {
                    int fd = events[i].data.fd;

                    // 读取 timerfd (必须读，否则会一直触发)
                    uint64_t exp;
                    read(fd, &exp, sizeof(uint64_t));

                    // 查找并执行回调
                    for (auto &task : tasks_)
                    {
                        if (task.fd == fd)
                        {
                            task.callback();
                            break;
                        }
                    }
                }
            }
        }

        void stop() { running_ = false; }

    private:
        int epoll_fd_;
        bool running_ = true;

        struct Task
        {
            int fd;
            std::function<void()> callback;
        };
        std::vector<Task> tasks_;
    };

} // namespace flow_scope
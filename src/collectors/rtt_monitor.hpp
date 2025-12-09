#pragma once
#include "monitor_base.hpp"
#include <iostream>
#include <cstring>
#include <chrono>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

namespace flow_scope
{

    // 定义一个简单的 ICMP 头部结构
    struct IcmpHeader
    {
        uint8_t type; // 8 = Request, 0 = Reply
        uint8_t code; // 0
        uint16_t checksum;
        uint16_t id;
        uint16_t sequence;
    };

    class RttMonitor : public MonitorBase
    {
    public:
        // target_ip: 要 Ping 的目标 IP，默认为 8.8.8.8 (Google DNS)
        RttMonitor(const std::string &target_ip = "8.8.8.8") : target_ip_(target_ip)
        {
            // 1. 创建 Raw Socket
            sockfd_ = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
            if (sockfd_ < 0)
            {
                perror("Socket creation failed (ROOT required?)");
                // 在实际工程中这里应该抛出异常或通过状态位通知
            }

            // 2. 设置接收超时 (1秒)，避免程序卡死
            struct timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

            // 3. 准备目标地址结构
            dest_addr_.sin_family = AF_INET;
            inet_pton(AF_INET, target_ip_.c_str(), &dest_addr_.sin_addr);

            // 使用进程ID作为 ICMP ID，便于识别
            packet_id_ = static_cast<uint16_t>(getpid() & 0xFFFF);
        }

        ~RttMonitor()
        {
            if (sockfd_ >= 0)
                close(sockfd_);
        }

        void collect(InterfaceMetrics &metrics) override
        {
            if (sockfd_ < 0)
            {
                metrics.rtt_ms = -1; // 错误状态
                return;
            }

            // --- 发送阶段 ---
            char send_buf[64]; // 数据包缓冲区
            memset(send_buf, 0, sizeof(send_buf));

            IcmpHeader *icmp = (IcmpHeader *)send_buf;
            icmp->type = 8; // ICMP Echo Request
            icmp->code = 0;
            icmp->id = htons(packet_id_);
            icmp->sequence = htons(seq_++);
            icmp->checksum = 0;
            // 计算校验和
            icmp->checksum = calculate_checksum((uint16_t *)icmp, sizeof(IcmpHeader));

            auto start_time = std::chrono::steady_clock::now();

            ssize_t sent = sendto(sockfd_, send_buf, sizeof(IcmpHeader), 0,
                                  (struct sockaddr *)&dest_addr_, sizeof(dest_addr_));
            if (sent <= 0)
            {
                metrics.packet_loss_rate = 1.0; // 发送失败算丢包
                return;
            }

            // --- 接收阶段 ---
            char recv_buf[1024];
            struct sockaddr_in from_addr;
            socklen_t addr_len = sizeof(from_addr);

            while (true)
            {
                ssize_t received = recvfrom(sockfd_, recv_buf, sizeof(recv_buf), 0,
                                            (struct sockaddr *)&from_addr, &addr_len);

                auto end_time = std::chrono::steady_clock::now();

                if (received <= 0)
                {
                    // 超时或错误
                    metrics.packet_loss_rate = 1.0; // 丢包
                    metrics.rtt_ms = 0;
                    break;
                }

                // --- 解析包 ---
                // 接收到的数据包含 IP 头 + ICMP 头
                // IP 头部通常是 20 字节
                struct ip *ip_hdr = (struct ip *)recv_buf;
                int ip_header_len = ip_hdr->ip_hl * 4;

                if (received < ip_header_len + (int)sizeof(IcmpHeader))
                    continue;

                IcmpHeader *icmp_reply = (IcmpHeader *)(recv_buf + ip_header_len);

                // 检查是否是我们的包 (ID 和 Sequence 匹配)
                if (icmp_reply->type == 0 && // Echo Reply
                    icmp_reply->id == htons(packet_id_) &&
                    icmp_reply->sequence == htons(seq_ - 1))
                {

                    // 计算 RTT
                    std::chrono::duration<double, std::milli> rtt = end_time - start_time;
                    metrics.rtt_ms = rtt.count();
                    metrics.packet_loss_rate = 0.0;
                    break; // 成功
                }
            }
        }

    private:
        int sockfd_;
        std::string target_ip_;
        struct sockaddr_in dest_addr_;
        uint16_t packet_id_;
        uint16_t seq_ = 0;

        // 标准网际校验和算法
        uint16_t calculate_checksum(uint16_t *b, int len)
        {
            uint32_t sum = 0;
            while (len > 1)
            {
                sum += *b++;
                len -= 2;
            }
            if (len == 1)
            {
                sum += *(uint8_t *)b;
            }
            sum = (sum >> 16) + (sum & 0xFFFF);
            sum += (sum >> 16);
            return ~sum;
        }
    };

} // namespace flow_scope
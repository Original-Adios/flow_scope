#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

// 定义许可协议 (必须是 GPL，否则内核拒绝加载)
char LICENSE[] SEC("license") = "GPL";

// --- 定义 Map (用于和用户态共享数据) ---
// 使用 Array Map，Key=0 的位置存储总重传数
struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, u32);
    __type(value, u64);
} tcp_retrans_counter SEC(".maps");

// --- 定义 Tracepoint Hook ---
// Hook 点: /sys/kernel/debug/tracing/events/tcp/tcp_retransmit_skb
// 当内核函数 tcp_retransmit_skb 执行时，触发此代码
SEC("tracepoint/tcp/tcp_retransmit_skb")
int handle_tcp_retransmit(void *ctx)
{

    // 1. 获取 Map 的 Key (0)
    u32 key = 0;
    u64 *val;

    // 2. 查找 Map 中的值
    val = bpf_map_lookup_elem(&tcp_retrans_counter, &key);
    if (!val)
    {
        return 0; // 理论上不会发生
    }

    // 3. 原子递增计数器
    // __sync_fetch_and_add 是 BPF 允许的原子操作
    __sync_fetch_and_add(val, 1);

    // 调试打印 (可以通过 sudo cat /sys/kernel/debug/tracing/trace_pipe 查看)
    // bpf_printk("FlowScope: TCP Retransmit detected!\n");

    return 0;
}
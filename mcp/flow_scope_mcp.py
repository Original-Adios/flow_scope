# from mcp.server.fastmcp import FastMCP
# import httpx
# import asyncio

# # 1. 创建 MCP 服务实例
# mcp = FastMCP("FlowScope Agent")

# # 2. 定义工具 (Tool)
# # @mcp.tool() 装饰器会让这个函数暴露给 LLM
# @mcp.tool()
# async def get_network_metrics() -> str:
#     """
#     获取当前系统的实时网络健康状况。
    
#     返回的数据包含：
#     - rtt_ms: ICMP 往返延迟 (毫秒)，衡量网络延迟。
#     - loss_rate: 丢包率 (0.0-1.0)，衡量连接稳定性。
#     - tcp_retrans: TCP 重传总计数。如果此数值在短时间内快速增加，表明发生严重拥塞。
#     - rx_bps/tx_bps: 实时接收/发送吞吐量 (Bytes/s)。
#     """
#     url = "http://localhost:8080/metrics"
    
#     try:
#         # 异步请求 C++ Agent 的接口
#         async with httpx.AsyncClient() as client:
#             resp = await client.get(url, timeout=2.0)
#             resp.raise_for_status()
            
#             # 直接返回 JSON 字符串，LLM 会自己解析结构
#             return resp.text
            
#     except httpx.ConnectError:
#         return "Error: 无法连接到 FlowScope Agent。请检查 'sudo ./flow_scope' 是否正在运行。"
#     except Exception as e:
#         return f"Error: 获取监控数据失败: {str(e)}"

# if __name__ == "__main__":
#     # 启动 MCP 服务 (标准输入输出模式)
#     mcp.run()


# 暴露为resource
from mcp.server.fastmcp import FastMCP
import httpx

# 1. 创建服务
mcp = FastMCP("FlowScope Agent")

# 定义资源的 URI 模式
RESOURCE_URI = "flowscope://metrics"

# 2. 定义资源 (Resource)
# 使用 @mcp.resource 装饰器
@mcp.resource(RESOURCE_URI)
async def get_system_metrics() -> str:
    """
    返回 FlowScope 的实时网络监控快照 (JSON)。
    包含 RTT、丢包率、TCP 重传数和吞吐量。
    """
    url = "http://localhost:8080/metrics"
    try:
        async with httpx.AsyncClient() as client:
            resp = await client.get(url, timeout=1.0)
            resp.raise_for_status()
            return resp.text
    except Exception as e:
        return f'{{"error": "{str(e)}"}}'

# -----------------------------------------------------------
# 💡 最佳实践：混合模式
# 我们可以同时保留 Tool，用于“诊断”等需要参数的操作
# -----------------------------------------------------------

@mcp.tool()
async def ping_target(ip: str) -> str:
    """
    (模拟) 使用 FlowScope 对指定 IP 进行 Ping 测试
    """
    # 这里未来可以调用 C++ 的扩展接口
    return f"Pinging {ip}... (Functionality pending implementation in C++)"

if __name__ == "__main__":
    mcp.run()
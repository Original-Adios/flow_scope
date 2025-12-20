import asyncio
import httpx
import uvicorn
from mcp.server import Server
from mcp.server.sse import SseServerTransport
from mcp.types import Resource, Tool, TextContent
from starlette.applications import Starlette
from starlette.responses import Response
from starlette.routing import Route
from starlette.middleware import Middleware
from starlette.middleware.cors import CORSMiddleware

# 1. 创建 MCP Server 实例
server = Server("flow-scope-agent")

# 定义常量
RESOURCE_URI = "flowscope://metrics"
AGENT_URL = "http://localhost:8080/metrics"

# =========================================================
#  Part 1: Resource 定义 (用于手动挂载上下文)
# =========================================================
@server.list_resources()
async def handle_list_resources():
    return [
        Resource(
            uri=RESOURCE_URI,
            name="Network Metrics Snapshot",
            description="当前网络监控数据的静态快照 (RTT, 丢包率, 重传数)。适合作为背景资料阅读。",
            mimeType="application/json",
        )
    ]

@server.read_resource()
async def handle_read_resource(uri: str):
    if uri != RESOURCE_URI:
        raise ValueError("Unknown resource")

    try:
        async with httpx.AsyncClient() as client:
            resp = await client.get(AGENT_URL, timeout=1.0)
            resp.raise_for_status()
            return resp.text
    except Exception as e:
        return f'{{"error": "无法连接到 C++ Agent: {str(e)}"}}'

# =========================================================
#  Part 2: Tool 定义 (用于 LLM 自动调用)
# =========================================================
@server.list_tools()
async def handle_list_tools():
    return [
        Tool(
            name="get_network_status",
            description="主动探测并获取最新的实时网络健康状况。当用户询问'网络怎么样'、'卡不卡'、'有没有丢包'时，必须调用此工具。",
            inputSchema={
                "type": "object",
                "properties": {}, # 此工具不需要参数
            }
        )
    ]

@server.call_tool()
async def handle_call_tool(name: str, arguments: dict | None):
    if name == "get_network_status":
        try:
            async with httpx.AsyncClient() as client:
                resp = await client.get(AGENT_URL, timeout=1.0)
                resp.raise_for_status()
                
                # Tool 必须返回 Content 列表
                return [
                    TextContent(
                        type="text",
                        text=resp.text
                    )
                ]
        except Exception as e:
            return [TextContent(type="text", text=f"Error fetching metrics: {str(e)}")]
    
    raise ValueError(f"Unknown tool: {name}")

# =========================================================
#  Part 3: Web 服务器逻辑 (Starlette + SSE + Custom Response)
# =========================================================
sse = SseServerTransport("/messages")

class MCPSseResponse(Response):
    """
    自定义 SSE 响应类。
    用于正确处理 ASGI 生命周期，避免 GET /sse 路由返回 None 导致报错。
    """
    def __init__(self, sse_transport, mcp_server):
        self.sse = sse_transport
        self.server = mcp_server
        super().__init__(media_type="text/event-stream")

    async def __call__(self, scope, receive, send):
        """ASGI 核心回调：接管 send 通道，建立 SSE 连接"""
        async with self.sse.connect_sse(scope, receive, send) as streams:
            await self.server.run(
                streams[0], 
                streams[1], 
                self.server.create_initialization_options()
            )

class MCPMessageResponse(Response):
    """
    自定义消息响应类。
    用于正确处理 POST /messages，避免 Starlette 报错 'NoneType is not callable'。
    """
    def __init__(self, sse_transport):
        self.sse = sse_transport
        super().__init__()

    async def __call__(self, scope, receive, send):
        """直接将控制权转交给 MCP 的 handle_post_message"""
        await self.sse.handle_post_message(scope, receive, send)

async def handle_sse(request):
    return MCPSseResponse(sse, server)

async def handle_messages(request):
    return MCPMessageResponse(sse)

# 创建 Web 应用
app = Starlette(
    debug=True,
    routes=[
        Route("/sse", endpoint=handle_sse),
        Route("/messages", endpoint=handle_messages, methods=["POST"]),
    ],
    middleware=[
        Middleware(
            CORSMiddleware,
            allow_origins=["*"],
            allow_methods=["*"],
            allow_headers=["*"],
        )
    ],
)

if __name__ == "__main__":
    print("🚀 Starting FlowScope MCP Server (Resource + Tool Mode)")
    print("👉 Endpoint: http://0.0.0.0:8000/sse")
    uvicorn.run(app, host="0.0.0.0", port=8000)
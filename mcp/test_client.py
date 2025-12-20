import subprocess
import json
import sys
import os

def run_json_rpc(process, request_dict):
    """辅助函数：发送请求并等待响应"""
    request_str = json.dumps(request_dict) + "\n"
    process.stdin.write(request_str)
    process.stdin.flush()
    
    # 读取响应
    while True:
        line = process.stdout.readline()
        if not line:
            return None
        
        try:
            data = json.loads(line)
            # 如果是对应的响应（ID匹配），返回
            if data.get("id") == request_dict.get("id"):
                return data
            # 忽略日志和其他通知，但打印出来方便调试
            if "method" in data and "log" in data["method"]:
                continue 
            # print(f"[Debug Ignored]: {line.strip()}")
        except json.JSONDecodeError:
            pass

def test_mcp_server():
    # 获取脚本所在的绝对路径
    script_dir = os.path.dirname(os.path.abspath(__file__))
    server_script = os.path.join(script_dir, "flow_scope_mcp.py")

    print(f">>> 启动 MCP Server: {server_script}")

    process = subprocess.Popen(
        [sys.executable, server_script],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=sys.stderr,
        text=True,
        bufsize=1
    )

    try:
        # ==========================================
        # 步骤 1: 发送 Initialize (握手)
        # ==========================================
        print(">>> [1/3] 发送握手请求 (initialize)...")
        init_req = {
            "jsonrpc": "2.0",
            "id": 0,
            "method": "initialize",
            "params": {
                "protocolVersion": "2024-11-05",
                "capabilities": {},
                "clientInfo": {"name": "test-client", "version": "1.0"}
            }
        }
        init_resp = run_json_rpc(process, init_req)
        print("✅ 握手成功！")

        # ==========================================
        # 步骤 2: 发送 Initialized 通知
        # ==========================================
        print(">>> [2/3] 确认握手 (notifications/initialized)...")
        initialized_notification = {
            "jsonrpc": "2.0",
            "method": "notifications/initialized"
        }
        # 通知不需要等待响应，直接发
        process.stdin.write(json.dumps(initialized_notification) + "\n")
        process.stdin.flush()

        # # ==========================================
        # # 步骤 3: 调用工具 (Call Tool)
        # # ==========================================
        # print(">>> [3/3] 调用工具 (tools/call: get_network_metrics)...")
        # tool_req = {
        #     "jsonrpc": "2.0",
        #     "id": 1,
        #     "method": "tools/call",
        #     "params": {
        #         "name": "get_network_metrics",
        #         "arguments": {}
        #     }
        # }
        
        # tool_resp = run_json_rpc(process, tool_req)
        
        # if tool_resp and "result" in tool_resp:
        #     print("\n🎉 最终结果 (来自 C++ Agent):")
        #     content = tool_resp["result"].get("content", [])
        #     for item in content:
        #         print(item.get("text"))
        # elif tool_resp and "error" in tool_resp:
        #     print("\n❌ 调用失败:")
        #     print(json.dumps(tool_resp["error"], indent=2, ensure_ascii=False))

        # ... (握手部分代码不变) ...

        # ==========================================
        # 步骤 3: 读取资源 (resources/read)
        # ==========================================
        print(">>> [3/3] 读取资源 (resources/read: flowscope://metrics)...")
        
        # 1. (可选) 先列出资源看看
        # list_req = {"jsonrpc": "2.0", "id": 2, "method": "resources/list"}
        # run_json_rpc(process, list_req)

        # 2. 读取指定资源
        resource_req = {
            "jsonrpc": "2.0",
            "id": 3,
            "method": "resources/read",
            "params": {
                "uri": "flowscope://metrics"
            }
        }
        
        res_resp = run_json_rpc(process, resource_req)
        
        if res_resp and "result" in res_resp:
            print("\n🎉 资源内容:")
            # 资源内容通常在 contents 列表中
            for item in res_resp["result"].get("contents", []):
                print(f"URI: {item['uri']}")
                print(f"Text: {item['text']}")
        elif res_resp and "error" in res_resp:
            print("\n❌ 读取失败:")
            print(json.dumps(res_resp["error"], indent=2))

    except KeyboardInterrupt:
        pass
    finally:
        process.terminate()

if __name__ == "__main__":
    test_mcp_server()
#!/usr/bin/env python3
import subprocess
import time
import threading
import socket
import os
import sys

# 目标地址（建议找一个公网IP或者局域网IP，不要用127.0.0.1，因为eBPF挂载点可能不过lo）
# 这里使用百度DNS作为示例，只发包不收包也行，只要触发重传
TARGET_IP = "192.0.2.1" 
TARGET_PORT = 53

def setup_packet_loss():
    print(f"[*] Setting up 30% packet loss to {TARGET_IP}...")
    # 使用 iptables 的 statistic 模块随机丢包
    subprocess.run(
        f"iptables -A OUTPUT -d {TARGET_IP} -m statistic --mode random --probability 0.3 -j DROP",
        shell=True, check=True
    )

def cleanup_packet_loss():
    print("[*] Cleaning up iptables rules...")
    subprocess.run(
        f"iptables -D OUTPUT -d {TARGET_IP} -m statistic --mode random --probability 0.3 -j DROP",
        shell=True, check=False
    )

def generate_traffic():
    print(f"[*] Connecting to {TARGET_IP}:{TARGET_PORT}...")
    try:
        # 使用 TCP 连接
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        
        # 尝试连接（SYN包可能会被丢弃并重传）
        try:
            sock.connect((TARGET_IP, TARGET_PORT))
        except socket.timeout:
            pass # 连接超时也是预期的
        except OSError:
            pass 

        # 如果连接成功，发送数据（数据包可能会被丢弃并重传）
        print("[*] Sending data...")
        for _ in range(5):
            try:
                sock.send(b"Hello WeakNet eBPF Test" * 100)
                time.sleep(0.5)
            except:
                break
        
        sock.close()
    except Exception as e:
        print(f"[!] Error: {e}")

if __name__ == "__main__":
    if os.geteuid() != 0:
        print("Error: Please run as root (for iptables)")
        sys.exit(1)

    try:
        setup_packet_loss()
        
        print("[*] Generating traffic in 3 seconds... Start retrans_tool now!")
        time.sleep(3)
        
        generate_traffic()
        
        print("[*] Traffic generation finished. Wait a few seconds for retransmits...")
        time.sleep(5)
        
    finally:
        cleanup_packet_loss()

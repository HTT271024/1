#!/usr/bin/env python3

import os
import subprocess
import time
import re
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from datetime import datetime

# 创建结果目录
results_dir = "results"
os.makedirs(results_dir, exist_ok=True)

def run_test(test_name, resp_size, data_rate="100Mbps", delay="20ms", error_rate=0, 
             n_streams=3, mixed_sizes=False, n_requests=100, sim_time=60):
    """运行一次HTTP/3测试并返回结果"""
    
    print(f"\n\n===== 运行测试: {test_name} =====")
    print(f"参数: respSize={resp_size}, dataRate={data_rate}, delay={delay}, errorRate={error_rate}, nStreams={n_streams}, mixedSizes={mixed_sizes}")
    
    # 创建测试结果目录
    test_dir = os.path.join(results_dir, test_name)
    os.makedirs(test_dir, exist_ok=True)
    
    # 构建命令
    cmd = [
        "./ns3", "run", "http3/http3", 
        "--", 
        f"--respSize={resp_size}",
        f"--dataRate={data_rate}",
        f"--delay={delay}",
        f"--errorRate={error_rate}",
        f"--nStreams={n_streams}",
        f"--nRequests={n_requests}",
        f"--simTime={sim_time}"
    ]
    
    if mixed_sizes:
        cmd.append("--mixedSizes=true")
    
    # 运行命令并捕获输出
    output_file = os.path.join(test_dir, "output.txt")
    cwnd_file = os.path.join(test_dir, "cwnd.csv")
    stream_completion_file = os.path.join(test_dir, "stream_completion.csv")
    
    with open(output_file, "w") as f_out:
        with open(cwnd_file, "w") as f_cwnd:
            with open(stream_completion_file, "w") as f_stream:
                # 写入CSV头
                f_cwnd.write("time,cwnd,bytes_in_flight\n")
                f_stream.write("time,stream_id,size\n")
                
                process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, 
                                         universal_newlines=True)
                
                # 处理输出
                for line in process.stdout:
                    f_out.write(line)
                    f_out.flush()
                    
                    # 提取拥塞窗口日志
                    if line.startswith("CWND_LOG"):
                        f_cwnd.write(line.replace("CWND_LOG,", ""))
                        f_cwnd.flush()
                    
                    # 提取流完成日志
                    elif line.startswith("STREAM_COMPLETED_LOG"):
                        f_stream.write(line.replace("STREAM_COMPLETED_LOG,", ""))
                        f_stream.flush()
                
                process.wait()
    
    # 分析结果
    throughput = extract_throughput(output_file)
    print(f"测试 {test_name} 完成，吞吐量: {throughput} Mbps")
    
    return {
        "test_name": test_name,
        "resp_size": resp_size,
        "data_rate": data_rate,
        "delay": delay,
        "error_rate": error_rate,
        "n_streams": n_streams,
        "mixed_sizes": mixed_sizes,
        "throughput": throughput,
        "output_file": output_file,
        "cwnd_file": cwnd_file,
        "stream_completion_file": stream_completion_file
    }

def extract_throughput(output_file):
    """从输出文件中提取吞吐量"""
    with open(output_file, "r") as f:
        content = f.read()
        match = re.search(r"Downlink throughput: (\d+\.\d+) Mbps", content)
        if match:
            return float(match.group(1))
    return None

def plot_throughput_scaling(results):
    """绘制吞吐量伸缩性测试结果"""
    sizes = [result["resp_size"] for result in results]
    sizes_kb = [size / 1024 for size in sizes]  # 转换为KB
    throughputs = [result["throughput"] for result in results]
    
    plt.figure(figsize=(10, 6))
    plt.plot(sizes_kb, throughputs, 'o-', linewidth=2)
    plt.xlabel('响应大小 (KB)')
    plt.ylabel('吞吐量 (Mbps)')
    plt.title('HTTP/3 吞吐量伸缩性测试')
    plt.grid(True)
    plt.savefig(os.path.join(results_dir, 'throughput_scaling.png'))
    plt.close()

def plot_cwnd(cwnd_file, output_file):
    """绘制拥塞窗口变化曲线"""
    try:
        df = pd.read_csv(cwnd_file)
        
        plt.figure(figsize=(12, 6))
        plt.plot(df['time'], df['cwnd'] / 1024, 'b-', linewidth=2, label='CWND (KB)')
        plt.plot(df['time'], df['bytes_in_flight'] / 1024, 'r-', linewidth=1, label='Bytes in Flight (KB)')
        plt.xlabel('时间 (秒)')
        plt.ylabel('窗口大小 (KB)')
        plt.title('QUIC 拥塞控制窗口变化')
        plt.legend()
        plt.grid(True)
        plt.savefig(output_file)
        plt.close()
    except Exception as e:
        print(f"绘制拥塞窗口图表时出错: {e}")

def plot_stream_completion(stream_file, output_file):
    """绘制流完成时间图表"""
    try:
        df = pd.read_csv(stream_file)
        
        # 按大小分类
        small_streams = df[df['size'] <= 10240]  # 10KB及以下
        medium_streams = df[(df['size'] > 10240) & (df['size'] <= 50240)]  # 10KB-50KB
        large_streams = df[df['size'] > 50240]  # 50KB以上
        
        plt.figure(figsize=(12, 6))
        
        if not small_streams.empty:
            plt.scatter(small_streams['time'], small_streams['size']/1024, 
                      color='green', marker='o', label='小文件 (≤10KB)', alpha=0.7)
        
        if not medium_streams.empty:
            plt.scatter(medium_streams['time'], medium_streams['size']/1024, 
                      color='blue', marker='s', label='中等文件 (10KB-50KB)', alpha=0.7)
        
        if not large_streams.empty:
            plt.scatter(large_streams['time'], large_streams['size']/1024, 
                      color='red', marker='x', label='大文件 (>50KB)', alpha=0.7)
        
        plt.xlabel('完成时间 (秒)')
        plt.ylabel('流大小 (KB)')
        plt.title('HTTP/3 流完成时间分布')
        plt.legend()
        plt.grid(True)
        plt.savefig(output_file)
        plt.close()
    except Exception as e:
        print(f"绘制流完成图表时出错: {e}")

def main():
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    print(f"HTTP/3 测试开始于 {timestamp}")
    
    # 测试1.1：吞吐量伸缩性测试
    throughput_results = []
    for size in [102400, 1048576, 5242880]:  # 100KB, 1MB, 5MB
        size_name = f"{size//1024}KB" if size < 1048576 else f"{size//1048576}MB"
        result = run_test(f"throughput_scaling_{size_name}", size)
        throughput_results.append(result)
    
    # 绘制吞吐量伸缩性测试结果
    plot_throughput_scaling(throughput_results)
    
    # 测试1.2：数据完整性测试
    run_test("data_integrity", 1048576, error_rate=0)
    
    # 测试2.1：拥塞控制行为可视化
    cc_result = run_test("congestion_control", 5242880, error_rate=0.01)
    plot_cwnd(cc_result["cwnd_file"], os.path.join(results_dir, "congestion_control_cwnd.png"))
    
    # 测试2.2：队头阻塞消除验证
    hol_result = run_test("hol_blocking", 1048576, error_rate=0.01, n_streams=50, mixed_sizes=True)
    plot_stream_completion(hol_result["stream_completion_file"], 
                         os.path.join(results_dir, "hol_blocking_stream_completion.png"))
    
    print("\n所有测试完成！结果保存在", results_dir)

if __name__ == "__main__":
    main() 
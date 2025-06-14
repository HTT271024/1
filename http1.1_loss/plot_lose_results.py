#!/usr/bin/env python3
import os
import re
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator

def parse_results(filename):
    """解析仿真结果文件"""
    with open(filename, 'r') as f:
        content = f.read()
    
    # 提取丢包率
    error_rate_match = re.search(r'RESULTS \(errorRate=([\d.]+)', content)
    if not error_rate_match:
        return None
    error_rate = float(error_rate_match.group(1))
    
    # 提取请求完成情况
    reqs_match = re.search(r'Total Requests Sent/Completed: (\d+)/(\d+)', content)
    if not reqs_match:
        return None
    
    total_sent = int(reqs_match.group(1))
    total_completed = int(reqs_match.group(2))
    
    # 提取吞吐量
    throughput_match = re.search(r'Throughput: ([\d.]+) Mbps', content)
    throughput = float(throughput_match.group(1)) if throughput_match else 0
    
    # 提取页面加载时间
    load_time_match = re.search(r'Page Load Time \(onLoad\): ([\d.]+) s', content)
    load_time = float(load_time_match.group(1)) if load_time_match else 0
    
    return {
        'error_rate': error_rate,
        'total_sent': total_sent,
        'total_completed': total_completed,
        'throughput': throughput,
        'load_time': load_time
    }

def plot_results(results_dir):
    """绘制结果图表"""
    # 收集所有结果
    data = []
    for filename in os.listdir(results_dir):
        if filename.startswith('lose_') and filename.endswith('.txt'):
            result = parse_results(os.path.join(results_dir, filename))
            if result:
                data.append(result)
    
    # 按丢包率排序
    data.sort(key=lambda x: x['error_rate'])
    
    # 准备数据
    error_rates = [d['error_rate'] for d in data]
    completion_rates = [d['total_completed']/d['total_sent']*100 for d in data]
    throughputs = [d['throughput'] for d in data]
    load_times = [d['load_time'] for d in data]
    
    # 创建图表
    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(10, 12))
    fig.suptitle('HTTP/1.1 Performance under Different Packet Loss Rates', fontsize=16)
    
    # 1. 完成率
    ax1.plot(error_rates, completion_rates, 'bo-', linewidth=2)
    ax1.set_xlabel('Packet Loss Rate')
    ax1.set_ylabel('Request Completion Rate (%)')
    ax1.set_title('Request Completion Rate vs Packet Loss')
    ax1.grid(True)
    ax1.xaxis.set_major_locator(MaxNLocator(5))
    
    # 2. 吞吐量
    ax2.plot(error_rates, throughputs, 'ro-', linewidth=2)
    ax2.set_xlabel('Packet Loss Rate')
    ax2.set_ylabel('Throughput (Mbps)')
    ax2.set_title('Throughput vs Packet Loss')
    ax2.grid(True)
    ax2.xaxis.set_major_locator(MaxNLocator(5))
    
    # 3. 加载时间
    ax3.plot(error_rates, load_times, 'go-', linewidth=2)
    ax3.set_xlabel('Packet Loss Rate')
    ax3.set_ylabel('Page Load Time (s)')
    ax3.set_title('Page Load Time vs Packet Loss')
    ax3.grid(True)
    ax3.xaxis.set_major_locator(MaxNLocator(5))
    
    plt.tight_layout()
    plt.savefig('http1.1_loss_results.png', dpi=300, bbox_inches='tight')
    plt.close()

if __name__ == '__main__':
    # 修改结果目录路径
    results_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'results')
    if not os.path.exists(results_dir):
        print(f"Error: Results directory '{results_dir}' not found!")
        exit(1)
    
    plot_results(results_dir)
    print("Results plotted and saved as 'http1.1_loss_results.png'") 
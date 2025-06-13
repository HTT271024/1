import matplotlib.pyplot as plt
import numpy as np
import json
import os
from datetime import datetime

def plot_results(results):
    # 创建图表
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 5))
    
    # 准备数据
    intervals = [r['interval'] for r in results]
    throughputs = [r['throughput'] for r in results]
    latencies = [r['latency'] for r in results]
    
    # 绘制吞吐量图
    ax1.plot(intervals, throughputs, 'bo-', label='Throughput', linewidth=2, markersize=8)
    ax1.set_xlabel('Request Interval (s)')
    ax1.set_ylabel('Throughput (Mbps)')
    ax1.set_title('Throughput vs Request Interval')
    ax1.grid(True)
    
    # 添加数据点标签
    for i, txt in enumerate(throughputs):
        ax1.annotate(f'{txt:.2f}', (intervals[i], throughputs[i]), 
                    xytext=(5, 5), textcoords='offset points')
    
    # 绘制延迟图
    ax2.plot(intervals, latencies, 'ro-', label='Latency', linewidth=2, markersize=8)
    ax2.set_xlabel('Request Interval (s)')
    ax2.set_ylabel('Latency (s)')
    ax2.set_title('Latency vs Request Interval')
    ax2.grid(True)
    
    # 添加数据点标签
    for i, txt in enumerate(latencies):
        ax2.annotate(f'{txt:.6f}', (intervals[i], latencies[i]), 
                    xytext=(5, 5), textcoords='offset points')
    
    # 保存图表
    plt.tight_layout()
    plt.savefig('experiment_results.png', dpi=300, bbox_inches='tight')
    plt.close()

def save_results(results):
    # 保存结果到JSON文件
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    filename = f'experiment_results_{timestamp}.json'
    
    with open(filename, 'w') as f:
        json.dump(results, f, indent=4)
    
    # 同时保存一个易读的文本文件
    txt_filename = f'experiment_results_{timestamp}.txt'
    with open(txt_filename, 'w') as f:
        f.write('HTTP/1.1 Experiment Results\n')
        f.write('=' * 50 + '\n\n')
        
        for result in results:
            f.write(f"Interval: {result['interval']}s\n")
            f.write(f"Completion Rate: {result['completion_rate']}\n")
            f.write(f"Average Latency: {result['latency']:.6f} s\n")
            f.write(f"Average Throughput: {result['throughput']:.2f} Mbps\n")
            f.write(f"Total Bytes Received: {result['total_bytes']}\n")
            f.write('-' * 50 + '\n\n')

def main():
    # 示例结果数据
    results = [
        {
            'interval': 0.01,
            'completion_rate': '5/5',
            'latency': 0.105155,
            'throughput': 5.64364,
            'total_bytes': 512000
        },
        {
            'interval': 0.02,
            'completion_rate': '5/5',
            'latency': 0.105072,
            'throughput': 4.42638,
            'total_bytes': 512000
        },
        {
            'interval': 0.05,
            'completion_rate': '5/5',
            'latency': 0.105155,
            'throughput': 5.64364,
            'total_bytes': 512000
        },
        {
            'interval': 0.1,
            'completion_rate': '5/5',
            'latency': 0.105072,
            'throughput': 4.42638,
            'total_bytes': 512000
        },
        {
            'interval': 0.2,
            'completion_rate': '5/5',
            'latency': 0.105072,
            'throughput': 4.42638,
            'total_bytes': 512000
        }
    ]
    
    # 生成图表
    plot_results(results)
    
    # 保存结果
    save_results(results)
    
    print("Results have been saved and plots have been generated!")

if __name__ == '__main__':
    main() 
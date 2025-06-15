#!/usr/bin/env python3
import os
import re
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

# 设置工作目录
os.chdir('/home/ekko/ns-3-dev-new/scratch/new')

def extract_metrics(file_path):
    with open(file_path, 'r') as f:
        content = f.read()
        
    # 提取完成率
    completion_match = re.search(r'客户端共收到响应数: (\d+)/(\d+)', content)
    if completion_match:
        received = int(completion_match.group(1))
        total = int(completion_match.group(2))
        completion_rate = received / total * 100
    else:
        completion_rate = 0
        
    # 提取延迟
    delay_match = re.search(r'平均延迟: ([\d.]+) s', content)
    delay = float(delay_match.group(1)) if delay_match else 0
    
    # 提取吞吐量
    throughput_match = re.search(r'平均吞吐量: ([\d.]+) Mbps', content)
    throughput = float(throughput_match.group(1)) if throughput_match else 0
    
    # 提取页面加载时间
    load_time_match = re.search(r'Page Load Time \(onLoad\): ([\d.]+) s', content)
    load_time = float(load_time_match.group(1)) if load_time_match else 0
    
    return {
        'completion_rate': completion_rate,
        'delay': delay,
        'throughput': throughput,
        'load_time': load_time
    }

def process_results():
    results = {
        'delay': [],
        'rate': [],
        'loss': [],
        'connections': []
    }
    
    results_dir = 'results'
    if not os.path.exists(results_dir):
        print(f"错误：{results_dir} 目录不存在")
        return results
    
    # 处理延迟测试结果
    for file in os.listdir(results_dir):
        if file.startswith('delay_') and file.endswith('.txt'):
            delay = float(file.replace('delay_', '').replace('ms.txt', ''))
            metrics = extract_metrics(os.path.join(results_dir, file))
            metrics['delay'] = delay
            results['delay'].append(metrics)
        elif file.startswith('rate_') and file.endswith('.txt'):
            rate = float(file.replace('rate_', '').replace('Mbps.txt', ''))
            metrics = extract_metrics(os.path.join(results_dir, file))
            metrics['rate'] = rate
            results['rate'].append(metrics)
        elif file.startswith('loss_') and file.endswith('.txt'):
            loss = float(file.replace('loss_', '').replace('.txt', ''))
            metrics = extract_metrics(os.path.join(results_dir, file))
            metrics['loss'] = loss
            results['loss'].append(metrics)
        elif file.startswith('conn_') and file.endswith('.txt'):
            conn = int(file.replace('conn_', '').replace('.txt', ''))
            metrics = extract_metrics(os.path.join(results_dir, file))
            metrics['connections'] = conn
            results['connections'].append(metrics)
    
    return results

def plot_results(results):
    # 设置图表风格
    sns.set_style("whitegrid")
    plt.rcParams['font.sans-serif'] = ['SimHei']  # 用来正常显示中文标签
    plt.rcParams['axes.unicode_minus'] = False  # 用来正常显示负号
    
    # 创建图表
    fig, axes = plt.subplots(2, 2, figsize=(15, 12))
    fig.suptitle('HTTP/1.1 Performance Analysis')
    
    # 延迟对性能的影响
    delay_df = pd.DataFrame(results['delay']).sort_values('delay')
    axes[0,0].plot(delay_df['delay'], delay_df['throughput'], marker='o')
    axes[0,0].set_title('Impact of Delay on Throughput')
    axes[0,0].set_xlabel('Delay (ms)')
    axes[0,0].set_ylabel('Throughput (Mbps)')
    
    # 带宽对性能的影响
    rate_df = pd.DataFrame(results['rate']).sort_values('rate')
    axes[0,1].plot(rate_df['rate'], rate_df['throughput'], marker='o')
    axes[0,1].set_title('Impact of Bandwidth on Throughput')
    axes[0,1].set_xlabel('Bandwidth (Mbps)')
    axes[0,1].set_ylabel('Throughput (Mbps)')
    
    # 丢包率对性能的影响
    loss_df = pd.DataFrame(results['loss']).sort_values('loss')
    axes[1,0].plot(loss_df['loss'], loss_df['completion_rate'], marker='o')
    axes[1,0].set_title('Impact of Packet Loss on Completion Rate')
    axes[1,0].set_xlabel('Packet Loss Rate')
    axes[1,0].set_ylabel('Completion Rate (%)')
    
    # 并发连接数对性能的影响
    conn_df = pd.DataFrame(results['connections']).sort_values('connections')
    axes[1,1].plot(conn_df['connections'], conn_df['load_time'], marker='o')
    axes[1,1].set_title('Impact of Connections on Page Load Time')
    axes[1,1].set_xlabel('Connections')
    axes[1,1].set_ylabel('Page Load Time (s)')
    
    # 保存图表
    plt.tight_layout()
    plt.savefig('results/performance_analysis.png')
    
    # 保存CSV数据
    for key, data in results.items():
        df = pd.DataFrame(data)
        df.to_csv(f'results/{key}_results.csv', index=False)

if __name__ == '__main__':
    results = process_results()
    plot_results(results)
    print("分析完成，结果保存在 results 目录下") 
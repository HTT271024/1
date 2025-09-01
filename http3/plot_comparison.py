#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
import os

# Set style
plt.style.use('seaborn-v0_8')
sns.set_palette("husl")
plt.rcParams['figure.figsize'] = (15, 10)

# Create a figure with subplots for comparison
fig, axes = plt.subplots(2, 3, figsize=(20, 12))
fig.suptitle('HTTP/2 vs HTTP/3 Performance Comparison', fontsize=16, fontweight='bold')

# Load data
try:
    h2_data = pd.read_csv('../http2/summary_bw_h2_correct.csv')
    h3_data = pd.read_csv('summary_bw_h3_correct.csv')
    h3_data['bandwidth_mbps'] = h3_data['bandwidth'].str.replace('Mbps', '').astype(float)
    
    # 数据清洗：确保数值列是纯数字
    num_cols = ["avg_throughput_Mbps","onload_s","avg_delay_s",
                "retx_count","retx_rate_per_s","jitter_s",
                "hol_events","hol_time_s"]
    for c in num_cols:
        if c in h3_data.columns:
            h3_data[c] = pd.to_numeric(h3_data[c], errors="coerce")
    
    # 清理百分号列
    if 'qpack_compression_percent' in h3_data.columns:
        h3_data['qpack_compression_percent'] = (
            h3_data['qpack_compression_percent'].astype(str).str.replace('%','', regex=False)
        )
        h3_data['qpack_compression_percent'] = pd.to_numeric(h3_data['qpack_compression_percent'], errors="coerce")
    
    # 过滤掉异常值 - 注意：CSV中的avg_throughput_Mbps已经是Mbps单位，不要再除以1e6
    h3_data = h3_data[h3_data['avg_throughput_Mbps'] > 0]
    
    # 1. Bandwidth vs Throughput Comparison
    axes[0,0].plot(h2_data['bandwidth_mbps'], h2_data['throughput_mbps'], 'o-', linewidth=2, markersize=8, color='blue', label='HTTP/2')
    axes[0,0].plot(h3_data['bandwidth_mbps'], h3_data['avg_throughput_Mbps'], 's-', linewidth=2, markersize=8, color='red', label='HTTP/3')
    axes[0,0].set_xlabel('Bandwidth (Mbps)')
    axes[0,0].set_ylabel('Throughput (Mbps)')
    axes[0,0].set_title('Bandwidth vs Throughput')
    axes[0,0].grid(True, alpha=0.3)
    axes[0,0].legend()

    # 2. Bandwidth vs Page Load Time Comparison
    axes[0,1].plot(h2_data['bandwidth_mbps'], h2_data['plt_s'], 'o-', linewidth=2, markersize=8, color='blue', label='HTTP/2')
    axes[0,1].plot(h3_data['bandwidth_mbps'], h3_data['onload_s'], 's-', linewidth=2, markersize=8, color='red', label='HTTP/3')
    axes[0,1].set_xlabel('Bandwidth (Mbps)')
    axes[0,1].set_ylabel('Page Load Time (s)')
    axes[0,1].set_title('Bandwidth vs Page Load Time')
    axes[0,1].grid(True, alpha=0.3)
    axes[0,1].legend()

    # 3. Bandwidth vs Average Delay Comparison
    axes[0,2].plot(h2_data['bandwidth_mbps'], h2_data['avg_delay_s'], 'o-', linewidth=2, markersize=8, color='blue', label='HTTP/2')
    axes[0,2].plot(h3_data['bandwidth_mbps'], h3_data['avg_delay_s'], 's-', linewidth=2, markersize=8, color='red', label='HTTP/3')
    axes[0,2].set_xlabel('Bandwidth (Mbps)')
    axes[0,2].set_ylabel('Average Response Time (s)')
    axes[0,2].set_title('Bandwidth vs Response Time')
    axes[0,2].grid(True, alpha=0.3)
    axes[0,2].legend()

    # 4. Bandwidth vs Jitter Comparison
    axes[1,0].plot(h2_data['bandwidth_mbps'], h2_data['jitter_s'], 'o-', linewidth=2, markersize=8, color='blue', label='HTTP/2')
    axes[1,0].plot(h3_data['bandwidth_mbps'], h3_data['jitter_s'], 's-', linewidth=2, markersize=8, color='red', label='HTTP/3')
    axes[1,0].set_xlabel('Bandwidth (Mbps)')
    axes[1,0].set_ylabel('Jitter (s)')
    axes[1,0].set_title('Bandwidth vs Jitter')
    axes[1,0].grid(True, alpha=0.3)
    axes[1,0].legend()

    # 5. Bandwidth vs Retransmissions Comparison
    axes[1,1].plot(h2_data['bandwidth_mbps'], h2_data['retx_count'], 'o-', linewidth=2, markersize=8, color='blue', label='HTTP/2')
    axes[1,1].plot(h3_data['bandwidth_mbps'], h3_data['retx_count'], 's-', linewidth=2, markersize=8, color='red', label='HTTP/3')
    axes[1,1].set_xlabel('Bandwidth (Mbps)')
    axes[1,1].set_ylabel('Retransmission Count')
    axes[1,1].set_title('Bandwidth vs Retransmissions')
    axes[1,1].grid(True, alpha=0.3)
    axes[1,1].legend()

    # 6. Protocol Comparison Summary
    axes[1,2].axis('off')
    
    # Calculate performance improvements
    h2_avg_throughput = h2_data['throughput_mbps'].mean()
    h3_avg_throughput = h3_data['avg_throughput_Mbps'].mean()
    throughput_improvement = ((h3_avg_throughput - h2_avg_throughput) / h2_avg_throughput) * 100
    
    h2_avg_plt = h2_data['plt_s'].mean()
    h3_avg_plt = h3_data['onload_s'].mean()
    plt_improvement = ((h2_avg_plt - h3_avg_plt) / h2_avg_plt) * 100
    
    summary_text = f"""
HTTP/2 vs HTTP/3 Comparison:

Performance Metrics:
   HTTP/2 Avg Throughput: {h2_avg_throughput:.2f} Mbps
   HTTP/3 Avg Throughput: {h3_avg_throughput:.2f} Mbps
   Throughput Change: {throughput_improvement:+.1f}%

   HTTP/2 Avg PLT: {h2_avg_plt:.2f} s
   HTTP/3 Avg PLT: {h3_avg_plt:.2f} s
   PLT Change: {plt_improvement:+.1f}%

Key Differences:
   HTTP/2: TCP-based, HPACK compression
   HTTP/3: QUIC-based, QPACK compression
   
   HTTP/2: Stream multiplexing over TCP
   HTTP/3: Native stream multiplexing
   
   HTTP/2: TCP-level HoL blocking
   HTTP/3: Reduced HoL blocking
   
   HTTP/2: TLS handshake overhead
   HTTP/3: 0-RTT connection establishment
"""

    axes[1,2].text(0.05, 0.95, summary_text, transform=axes[1,2].transAxes, fontsize=10,
                   verticalalignment='top', fontfamily='monospace',
                   bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.8))

except Exception as e:
    print(f"Error loading data: {e}")
    axes[0,0].text(0.5, 0.5, f'Error loading data: {e}', ha='center', va='center', transform=axes[0,0].transAxes)

plt.tight_layout()
plt.savefig('http2_vs_http3_comparison.png', dpi=300, bbox_inches='tight')
plt.savefig('http2_vs_http3_comparison.pdf', bbox_inches='tight')
print("Comparison plots saved as 'http2_vs_http3_comparison.png' and 'http2_vs_http3_comparison.pdf'")

# Create individual comparison plot
try:
    plt.figure(figsize=(12, 8))
    plt.plot(h2_data['bandwidth_mbps'], h2_data['throughput_mbps'], 'o-', linewidth=3, markersize=10, label='HTTP/2', color='blue')
    plt.plot(h3_data['bandwidth_mbps'], h3_data['avg_throughput_Mbps'], 's-', linewidth=3, markersize=10, label='HTTP/3', color='red')
    plt.xlabel('Bandwidth (Mbps)', fontsize=12)
    plt.ylabel('Throughput (Mbps)', fontsize=12)
    plt.title('HTTP/2 vs HTTP/3: Bandwidth vs Throughput', fontsize=14, fontweight='bold')
    plt.grid(True, alpha=0.3)
    plt.legend(fontsize=12)

    # Add annotations
    for i, (bw, thr_h2, thr_h3) in enumerate(zip(h2_data['bandwidth_mbps'], h2_data['throughput_mbps'], h3_data['avg_throughput_Mbps'])):
        plt.annotate(f'H2: {thr_h2:.2f}', (bw, thr_h2), textcoords="offset points", xytext=(0,10), ha='center', fontsize=8)
        plt.annotate(f'H3: {thr_h3:.2f}', (bw, thr_h3), textcoords="offset points", xytext=(0,-15), ha='center', fontsize=8)

    plt.tight_layout()
    plt.savefig('http2_vs_http3_throughput.png', dpi=300, bbox_inches='tight')
    print("Individual comparison plot saved as 'http2_vs_http3_throughput.png'")
    
except Exception as e:
    print(f"Error creating individual comparison plot: {e}")

plt.show() 
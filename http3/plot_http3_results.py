#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
import os

# Set style
plt.style.use('seaborn-v0_8')
sns.set_palette("husl")
plt.rcParams['figure.figsize'] = (12, 8)

# Create a figure with subplots
fig, axes = plt.subplots(2, 3, figsize=(18, 12))
fig.suptitle('HTTP/3 Performance Analysis', fontsize=16, fontweight='bold')

# 1. Bandwidth vs Throughput
try:
    # Convert bandwidth strings to numeric values
    bw_data = pd.read_csv('summary_bw_h3.csv')
    # 清理数据：移除可能的换行符和格式问题
    bw_data['bandwidth'] = bw_data['bandwidth'].str.strip()
    bw_data['bandwidth_mbps'] = bw_data['bandwidth'].str.replace('Mbps', '').astype(float)
    
    # 数据清洗：确保数值列是纯数字
    num_cols = ["avg_throughput_Mbps","onload_s","avg_delay_s",
                "retx_count","retx_rate_per_s","jitter_s",
                "hol_events","hol_time_s"]
    for c in num_cols:
        if c in bw_data.columns:
            bw_data[c] = pd.to_numeric(bw_data[c], errors="coerce")
    
    # 清理百分号列
    if 'qpack_compression_percent' in bw_data.columns:
        bw_data['qpack_compression_percent'] = (
            bw_data['qpack_compression_percent'].astype(str).str.replace('%','', regex=False)
        )
        bw_data['qpack_compression_percent'] = pd.to_numeric(bw_data['qpack_compression_percent'], errors="coerce")
    
    # 过滤掉异常值 - 注意：CSV中的avg_throughput_Mbps已经是Mbps单位，不要再除以1e6
    bw_data = bw_data[bw_data['avg_throughput_Mbps'] > 0]
    
    axes[0,0].plot(bw_data['bandwidth_mbps'], bw_data['avg_throughput_Mbps'], 'o-', linewidth=2, markersize=8, color='blue')
    axes[0,0].set_xlabel('Bandwidth (Mbps)')
    axes[0,0].set_ylabel('Throughput (Mbps)')
    axes[0,0].set_title('HTTP/3: Bandwidth vs Throughput')
    axes[0,0].grid(True, alpha=0.3)
    # Add trend line
    z = np.polyfit(bw_data['bandwidth_mbps'], bw_data['avg_throughput_Mbps'], 1)
    p = np.poly1d(z)
    axes[0,0].plot(bw_data['bandwidth_mbps'], p(bw_data['bandwidth_mbps']), "--", alpha=0.8, color='red')
except Exception as e:
    axes[0,0].text(0.5, 0.5, f'Error loading bandwidth data: {e}', ha='center', va='center', transform=axes[0,0].transAxes)

# 2. Bandwidth vs Page Load Time
try:
    axes[0,1].plot(bw_data['bandwidth_mbps'], bw_data['onload_s'], 's-', linewidth=2, markersize=8, color='orange')
    axes[0,1].set_xlabel('Bandwidth (Mbps)')
    axes[0,1].set_ylabel('Page Load Time (s)')
    axes[0,1].set_title('HTTP/3: Bandwidth vs Page Load Time')
    axes[0,1].grid(True, alpha=0.3)
except Exception as e:
    axes[0,1].text(0.5, 0.5, f'Error: {e}', ha='center', va='center', transform=axes[0,1].transAxes)

# 3. Bandwidth vs Average Delay
try:
    axes[0,2].plot(bw_data['bandwidth_mbps'], bw_data['avg_delay_s'], '^-', linewidth=2, markersize=8, color='green')
    axes[0,2].set_xlabel('Bandwidth (Mbps)')
    axes[0,2].set_ylabel('Average Response Time (s)')
    axes[0,2].set_title('HTTP/3: Bandwidth vs Response Time')
    axes[0,2].grid(True, alpha=0.3)
except Exception as e:
    axes[0,2].text(0.5, 0.5, f'Error loading delay data: {e}', ha='center', va='center', transform=axes[0,2].transAxes)

# 4. Bandwidth vs Jitter
try:
    axes[1,0].plot(bw_data['bandwidth_mbps'], bw_data['jitter_s'], 'v-', linewidth=2, markersize=8, color='purple')
    axes[1,0].set_xlabel('Bandwidth (Mbps)')
    axes[1,0].set_ylabel('Jitter (s)')
    axes[1,0].set_title('HTTP/3: Bandwidth vs Jitter')
    axes[1,0].grid(True, alpha=0.3)
except Exception as e:
    axes[1,0].text(0.5, 0.5, f'Error: {e}', ha='center', va='center', transform=axes[1,0].transAxes)

# 5. Bandwidth vs Retransmissions and HoL Events
try:
    ax5 = axes[1,1]
    ax5_twin = ax5.twinx()
    
    line1 = ax5.plot(bw_data['bandwidth_mbps'], bw_data['retx_count'], 'o-', linewidth=2, markersize=8, color='blue', label='Retransmissions')
    line2 = ax5_twin.plot(bw_data['bandwidth_mbps'], bw_data['hol_events'], 's-', linewidth=2, markersize=8, color='red', label='HoL Events')
    
    ax5.set_xlabel('Bandwidth (Mbps)')
    ax5.set_ylabel('Retransmission Count', color='blue')
    ax5_twin.set_ylabel('HoL Events Count', color='red')
    ax5.set_title('HTTP/3: Bandwidth vs Error Events')
    ax5.grid(True, alpha=0.3)
    
    # Combine legends
    lines1, labels1 = ax5.get_legend_handles_labels()
    lines2, labels2 = ax5_twin.get_legend_handles_labels()
    ax5.legend(lines1 + lines2, labels1 + labels2, loc='center right')
    
except Exception as e:
    axes[1,1].text(0.5, 0.5, f'Error loading error data: {e}', ha='center', va='center', transform=axes[1,1].transAxes)

# 6. HTTP/3 Specific Features
axes[1,2].axis('off')
summary_text = """
HTTP/3 FEATURES:

QUIC Transport:
   • UDP-based multiplexing
   • Connection-level flow control
   • Stream-level flow control
   • Built-in congestion control

Stream Multiplexing:
   • Concurrent streams: 3
   • Frame interleaving: 1200B chunks
   • Tick interval: 500μs
   • Stream offset tracking

QPACK Compression:
   • Base header: 200B
   • Compression ratio: 70%
   • Bandwidth savings tracked

Performance Metrics:
   • QUIC retransmissions
   • Stream completion tracking
   • RFC3550 jitter estimation
   • HoL blocking detection

Advanced Features:
   • Frame chunking
   • Tick-based scheduling
   • Error rate simulation
   • Server push support
"""

axes[1,2].text(0.05, 0.95, summary_text, transform=axes[1,2].transAxes, fontsize=10,
               verticalalignment='top', fontfamily='monospace',
               bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.8))

plt.tight_layout()
plt.savefig('http3_performance_analysis.png', dpi=300, bbox_inches='tight')
plt.savefig('http3_performance_analysis.pdf', bbox_inches='tight')
print("✅ Plots saved as 'http3_performance_analysis.png' and 'http3_performance_analysis.pdf'")

# Also create individual plots
try:
    plt.figure(figsize=(10, 6))
    plt.plot(bw_data['bandwidth_mbps'], bw_data['avg_throughput_Mbps'], 'o-', linewidth=3, markersize=10, label='HTTP/3 Throughput', color='blue')
    plt.xlabel('Bandwidth (Mbps)', fontsize=12)
    plt.ylabel('Throughput (Mbps)', fontsize=12)
    plt.title('HTTP/3 Bandwidth vs Throughput', fontsize=14, fontweight='bold')
    plt.grid(True, alpha=0.3)
    plt.legend()

    # Add annotations
    for i, (bw, thr) in enumerate(zip(bw_data['bandwidth_mbps'], bw_data['avg_throughput_Mbps'])):
        plt.annotate(f'{thr:.2f}', (bw, thr), textcoords="offset points", xytext=(0,10), ha='center')

    plt.tight_layout()
    plt.savefig('http3_bandwidth_vs_throughput.png', dpi=300, bbox_inches='tight')
    print("✅ Individual plot saved as 'http3_bandwidth_vs_throughput.png'")
    
except Exception as e:
    print(f"❌ Error creating individual plot: {e}")

plt.show() 
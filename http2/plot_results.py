#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np

# Set style
plt.style.use('seaborn-v0_8')
sns.set_palette("husl")
plt.rcParams['figure.figsize'] = (12, 8)

# Create a figure with subplots
fig, axes = plt.subplots(2, 3, figsize=(18, 12))
fig.suptitle('HTTP/2 Performance Analysis', fontsize=16, fontweight='bold')

# 1. Bandwidth vs Throughput
try:
    bw_data = pd.read_csv('summary_bw_h2_correct.csv')
    axes[0,0].plot(bw_data['bandwidth_mbps'], bw_data['throughput_mbps'], 'o-', linewidth=2, markersize=8, color='blue')
    axes[0,0].set_xlabel('Bandwidth (Mbps)')
    axes[0,0].set_ylabel('Throughput (Mbps)')
    axes[0,0].set_title('HTTP/2: Bandwidth vs Throughput')
    axes[0,0].grid(True, alpha=0.3)
    # Add trend line
    z = np.polyfit(bw_data['bandwidth_mbps'], bw_data['throughput_mbps'], 1)
    p = np.poly1d(z)
    axes[0,0].plot(bw_data['bandwidth_mbps'], p(bw_data['bandwidth_mbps']), "--", alpha=0.8, color='red')
except Exception as e:
    axes[0,0].text(0.5, 0.5, f'Error loading bandwidth data: {e}', ha='center', va='center', transform=axes[0,0].transAxes)

# 2. Bandwidth vs Page Load Time
try:
    axes[0,1].plot(bw_data['bandwidth_mbps'], bw_data['plt_s'], 's-', linewidth=2, markersize=8, color='orange')
    axes[0,1].set_xlabel('Bandwidth (Mbps)')
    axes[0,1].set_ylabel('Page Load Time (s)')
    axes[0,1].set_title('HTTP/2: Bandwidth vs Page Load Time')
    axes[0,1].grid(True, alpha=0.3)
except Exception as e:
    axes[0,1].text(0.5, 0.5, f'Error: {e}', ha='center', va='center', transform=axes[0,1].transAxes)

# 3. Latency vs Response Time
try:
    lat_data = pd.read_csv('latency_sweep_h2_correct.csv')
    axes[0,2].plot(lat_data['latency_ms'], lat_data['avg_delay_s'], '^-', linewidth=2, markersize=8, color='green')
    axes[0,2].set_xlabel('Network Latency (ms)')
    axes[0,2].set_ylabel('Average Response Time (s)')
    axes[0,2].set_title('HTTP/2: Latency vs Response Time')
    axes[0,2].grid(True, alpha=0.3)
except Exception as e:
    axes[0,2].text(0.5, 0.5, f'Error loading latency data: {e}', ha='center', va='center', transform=axes[0,2].transAxes)

# 4. Latency vs Throughput
try:
    axes[1,0].plot(lat_data['latency_ms'], lat_data['throughput_mbps'], 'v-', linewidth=2, markersize=8, color='purple')
    axes[1,0].set_xlabel('Network Latency (ms)')
    axes[1,0].set_ylabel('Throughput (Mbps)')
    axes[1,0].set_title('HTTP/2: Latency vs Throughput')
    axes[1,0].grid(True, alpha=0.3)
except Exception as e:
    axes[1,0].text(0.5, 0.5, f'Error: {e}', ha='center', va='center', transform=axes[1,0].transAxes)

# 5. Loss Rate vs Throughput and Retransmissions
try:
    loss_data = pd.read_csv('loss_sweep_h2_correct.csv')
    ax5 = axes[1,1]
    ax5_twin = ax5.twinx()
    
    line1 = ax5.plot(loss_data['loss_rate']*100, loss_data['throughput_mbps'], 'o-', linewidth=2, markersize=8, color='blue', label='Throughput')
    line2 = ax5_twin.plot(loss_data['loss_rate']*100, loss_data['retx_count'], 's-', linewidth=2, markersize=8, color='red', label='Retransmissions')
    
    ax5.set_xlabel('Packet Loss Rate (%)')
    ax5.set_ylabel('Throughput (Mbps)', color='blue')
    ax5_twin.set_ylabel('Retransmission Count', color='red')
    ax5.set_title('HTTP/2: Loss Rate Impact')
    ax5.grid(True, alpha=0.3)
    
    # Combine legends
    lines1, labels1 = ax5.get_legend_handles_labels()
    lines2, labels2 = ax5_twin.get_legend_handles_labels()
    ax5.legend(lines1 + lines2, labels1 + labels2, loc='center right')
    
except Exception as e:
    axes[1,1].text(0.5, 0.5, f'Error loading loss data: {e}', ha='center', va='center', transform=axes[1,1].transAxes)

# 6. HTTP/2 Specific Features
axes[1,2].axis('off')
summary_text = """
🚀 HTTP/2 FEATURES:

✅ Multiplexing:
   • Concurrent streams: 3
   • Frame interleaving: 1200B chunks
   • Tick interval: 500μs

✅ Flow Control:
   • Connection window: 32MB
   • Stream window: 32MB
   • Stream-level blocking detection

✅ HPACK Compression:
   • Base header: 200B
   • Compression ratio: 70%
   • Bandwidth savings tracked

✅ Performance Metrics:
   • TCP-level HoL stall
   • Stream completion tracking
   • RFC3550 jitter estimation

✅ Advanced Features:
   • Frame chunking
   • Tick-based scheduling
   • Error rate simulation
"""

axes[1,2].text(0.05, 0.95, summary_text, transform=axes[1,2].transAxes, fontsize=10,
               verticalalignment='top', fontfamily='monospace',
               bbox=dict(boxstyle='round', facecolor='lightgreen', alpha=0.8))

plt.tight_layout()
plt.savefig('http2_performance_analysis.png', dpi=300, bbox_inches='tight')
plt.savefig('http2_performance_analysis.pdf', bbox_inches='tight')
print("✅ Plots saved as 'http2_performance_analysis.png' and 'http2_performance_analysis.pdf'")

# Also create individual plots
try:
    plt.figure(figsize=(10, 6))
    plt.plot(bw_data['bandwidth_mbps'], bw_data['throughput_mbps'], 'o-', linewidth=3, markersize=10, label='HTTP/2 Throughput', color='blue')
    plt.xlabel('Bandwidth (Mbps)', fontsize=12)
    plt.ylabel('Throughput (Mbps)', fontsize=12)
    plt.title('HTTP/2 Bandwidth vs Throughput', fontsize=14, fontweight='bold')
    plt.grid(True, alpha=0.3)
    plt.legend()

    # Add annotations
    for i, (bw, thr) in enumerate(zip(bw_data['bandwidth_mbps'], bw_data['throughput_mbps'])):
        plt.annotate(f'{thr:.2f}', (bw, thr), textcoords="offset points", xytext=(0,10), ha='center')

    plt.tight_layout()
    plt.savefig('http2_bandwidth_vs_throughput.png', dpi=300, bbox_inches='tight')
    print("✅ Individual plot saved as 'http2_bandwidth_vs_throughput.png'")
    
except Exception as e:
    print(f"❌ Error creating individual plot: {e}")

plt.show() 
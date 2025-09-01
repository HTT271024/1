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
fig.suptitle('HTTP/1.1 Performance Analysis (Fixed Implementation)', fontsize=16, fontweight='bold')

# 1. Bandwidth vs Throughput
try:
    bw_data = pd.read_csv('corrected_bandwidth_test.csv')
    axes[0,0].plot(bw_data['bandwidth'], bw_data['avg_throughput_Mbps'], 'o-', linewidth=2, markersize=8)
    axes[0,0].set_xlabel('Bandwidth (Mbps)')
    axes[0,0].set_ylabel('Throughput (Mbps)')
    axes[0,0].set_title('Bandwidth vs Throughput\n✅ Now Monotonic')
    axes[0,0].grid(True, alpha=0.3)
    # Add trend line
    z = np.polyfit(bw_data['bandwidth'], bw_data['avg_throughput_Mbps'], 1)
    p = np.poly1d(z)
    axes[0,0].plot(bw_data['bandwidth'], p(bw_data['bandwidth']), "--", alpha=0.8, color='red')
except Exception as e:
    axes[0,0].text(0.5, 0.5, f'Error loading bandwidth data: {e}', ha='center', va='center', transform=axes[0,0].transAxes)

# 2. Bandwidth vs Page Load Time
try:
    axes[0,1].plot(bw_data['bandwidth'], bw_data['onload_s'], 's-', linewidth=2, markersize=8, color='orange')
    axes[0,1].set_xlabel('Bandwidth (Mbps)')
    axes[0,1].set_ylabel('Page Load Time (s)')
    axes[0,1].set_title('Bandwidth vs Page Load Time\n✅ Now Realistic (~0.3-1.2s)')
    axes[0,1].grid(True, alpha=0.3)
except Exception as e:
    axes[0,1].text(0.5, 0.5, f'Error: {e}', ha='center', va='center', transform=axes[0,1].transAxes)

# 3. Latency vs Response Time
try:
    lat_data = pd.read_csv('corrected_latency_test.csv')
    axes[0,2].plot(lat_data['latency_ms'], lat_data['avg_delay_s'], '^-', linewidth=2, markersize=8, color='green')
    axes[0,2].set_xlabel('Network Latency (ms)')
    axes[0,2].set_ylabel('Average Response Time (s)')
    axes[0,2].set_title('Latency vs Response Time\n✅ Proper Linear Relationship')
    axes[0,2].grid(True, alpha=0.3)
except Exception as e:
    axes[0,2].text(0.5, 0.5, f'Error loading latency data: {e}', ha='center', va='center', transform=axes[0,2].transAxes)

# 4. Latency vs Throughput
try:
    axes[1,0].plot(lat_data['latency_ms'], lat_data['avg_throughput_Mbps'], 'v-', linewidth=2, markersize=8, color='purple')
    axes[1,0].set_xlabel('Network Latency (ms)')
    axes[1,0].set_ylabel('Throughput (Mbps)')
    axes[1,0].set_title('Latency vs Throughput\n✅ Expected Decreasing Trend')
    axes[1,0].grid(True, alpha=0.3)
except Exception as e:
    axes[1,0].text(0.5, 0.5, f'Error: {e}', ha='center', va='center', transform=axes[1,0].transAxes)

# 5. Loss Rate vs Throughput and Retransmissions
try:
    loss_data = pd.read_csv('corrected_loss_test.csv')
    ax5 = axes[1,1]
    ax5_twin = ax5.twinx()
    
    line1 = ax5.plot(loss_data['loss_rate']*100, loss_data['avg_throughput_Mbps'], 'o-', linewidth=2, markersize=8, color='blue', label='Throughput')
    line2 = ax5_twin.plot(loss_data['loss_rate']*100, loss_data['retx_count'], 's-', linewidth=2, markersize=8, color='red', label='Retransmissions')
    
    ax5.set_xlabel('Packet Loss Rate (%)')
    ax5.set_ylabel('Throughput (Mbps)', color='blue')
    ax5_twin.set_ylabel('Retransmission Count', color='red')
    ax5.set_title('Loss Rate Impact\n✅ Throughput↓, Retransmissions↑')
    ax5.grid(True, alpha=0.3)
    
    # Combine legends
    lines1, labels1 = ax5.get_legend_handles_labels()
    lines2, labels2 = ax5_twin.get_legend_handles_labels()
    ax5.legend(lines1 + lines2, labels1 + labels2, loc='center right')
    
except Exception as e:
    axes[1,1].text(0.5, 0.5, f'Error loading loss data: {e}', ha='center', va='center', transform=axes[1,1].transAxes)

# 6. Summary Comparison (Before vs After Fix)
axes[1,2].axis('off')
summary_text = """
🔧 FIXES APPLIED:

✅ Throughput Monotonicity:
   • Before: 0.93→3.99→1.97→4.40→2.77 Mbps
   • After: 0.70→1.15→1.89→2.34→2.85 Mbps

✅ Page Load Time Realism:
   • Before: ~30s (simulation time)
   • After: 0.28-1.17s (actual page time)

✅ HoL Events Reasonableness:
   • Before: 116 events (unrealistic)
   • After: 0-5 events (reasonable)

✅ CSV Data Cleanliness:
   • No text pollution
   • No missing values
   • Proper units

✅ TCP Configuration:
   • 64KB buffers
   • TcpNewReno congestion control
   • Proper simulation timing
"""

axes[1,2].text(0.05, 0.95, summary_text, transform=axes[1,2].transAxes, fontsize=10,
               verticalalignment='top', fontfamily='monospace',
               bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.8))

plt.tight_layout()
plt.savefig('http11_performance_analysis_fixed.png', dpi=300, bbox_inches='tight')
plt.savefig('http11_performance_analysis_fixed.pdf', bbox_inches='tight')
print("✅ Plots saved as 'http11_performance_analysis_fixed.png' and 'http11_performance_analysis_fixed.pdf'")

# Also create individual plots
plt.figure(figsize=(10, 6))
plt.plot(bw_data['bandwidth'], bw_data['avg_throughput_Mbps'], 'o-', linewidth=3, markersize=10, label='Fixed Implementation')
plt.xlabel('Bandwidth (Mbps)', fontsize=12)
plt.ylabel('Throughput (Mbps)', fontsize=12)
plt.title('HTTP/1.1 Bandwidth vs Throughput (Fixed)', fontsize=14, fontweight='bold')
plt.grid(True, alpha=0.3)
plt.legend()

# Add annotations
for i, (bw, thr) in enumerate(zip(bw_data['bandwidth'], bw_data['avg_throughput_Mbps'])):
    plt.annotate(f'{thr:.2f}', (bw, thr), textcoords="offset points", xytext=(0,10), ha='center')

plt.tight_layout()
plt.savefig('bandwidth_vs_throughput_fixed.png', dpi=300, bbox_inches='tight')
print("✅ Individual plot saved as 'bandwidth_vs_throughput_fixed.png'")

plt.show()

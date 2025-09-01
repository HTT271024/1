#!/usr/bin/env python3

import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np

# Set style
plt.style.use('default')
sns.set_palette("husl")

# Load data
print("Loading HTTP/1.1 data...")
h1 = pd.read_csv('../http1.1/corrected_bandwidth_test.csv')

print("Loading HTTP/2 data...")
h2 = pd.read_csv('../http2/summary_bw_h2_correct.csv')

print("Loading HTTP/3 data...")
h3 = pd.read_csv('summary_bw_h3.csv')

# Normalize column names for HTTP/1.1 (already correct)
# h1 columns: bandwidth, avg_throughput_Mbps, onload_s

# Normalize column names for HTTP/2
h2_cols = {
    'bandwidth_mbps': 'bandwidth',
    'throughput_mbps': 'avg_throughput_Mbps',
    'plt_s': 'onload_s',
    'avg_delay_s': 'avg_response_time_s',
    'retx_count': 'retransmissions'
}
h2 = h2.rename(columns=h2_cols)

# Normalize column names for HTTP/3
h3_cols = {
    'retx_count': 'retransmissions',
    'avg_delay_s': 'avg_response_time_s'
}
h3 = h3.rename(columns=h3_cols)

# Ensure numeric types for HTTP/1.1
numeric_cols_h1 = ['bandwidth', 'avg_throughput_Mbps', 'onload_s']
for col in numeric_cols_h1:
    if col in h1.columns:
        h1[col] = pd.to_numeric(h1[col], errors='coerce')

# Ensure numeric types for HTTP/2
numeric_cols_h2 = ['bandwidth', 'avg_throughput_Mbps', 'onload_s', 'avg_response_time_s', 'jitter_s', 'retransmissions', 'hpack_compression_percent']
for col in numeric_cols_h2:
    if col in h2.columns:
        h2[col] = pd.to_numeric(h2[col], errors='coerce')

# Ensure numeric types for HTTP/3
numeric_cols_h3 = ['bandwidth', 'avg_throughput_Mbps', 'onload_s', 'avg_response_time_s', 'jitter_s', 'retransmissions', 'qpack_compression_percent']
for col in numeric_cols_h3:
    if col in h3.columns:
        if col == 'qpack_compression_percent':
            # Clean % character if present
            h3[col] = h3[col].astype(str).str.replace('%', '')
        elif col == 'bandwidth':
            # Convert "1Mbps" format to numeric
            h3[col] = h3[col].astype(str).str.replace('Mbps', '').astype(float)
        h3[col] = pd.to_numeric(h3[col], errors='coerce')

# Sort by bandwidth
h1 = h1.sort_values('bandwidth')
h2 = h2.sort_values('bandwidth')
h3 = h3.sort_values('bandwidth')

# Create the plot
fig, axes = plt.subplots(2, 3, figsize=(18, 12))
fig.suptitle('HTTP/1.1 vs HTTP/2 vs HTTP/3 Performance Comparison', fontsize=16, fontweight='bold')

# Define colors and styles for each protocol
colors = {'HTTP/1.1': '#FF6B6B', 'HTTP/2': '#4ECDC4', 'HTTP/3': '#DC143C'}  # HTTP/3 uses crimson red
styles = {
    'HTTP/1.1': {'linestyle': '-', 'marker': 'o', 'markersize': 6, 'linewidth': 2},
    'HTTP/2': {'linestyle': '-', 'marker': 's', 'markersize': 6, 'linewidth': 2},
    'HTTP/3': {'linestyle': '--', 'marker': 'o', 'markersize': 10, 'linewidth': 3}  # Dashed line, large circles, thick line
}

# 1. Throughput vs Bandwidth
ax = axes[0, 0]
ax.plot(h1['bandwidth'], h1['avg_throughput_Mbps'], color=colors['HTTP/1.1'], label='HTTP/1.1', **styles['HTTP/1.1'])
ax.plot(h2['bandwidth'], h2['avg_throughput_Mbps'], color=colors['HTTP/2'], label='HTTP/2', **styles['HTTP/2'])
ax.plot(h3['bandwidth'], h3['avg_throughput_Mbps'], color=colors['HTTP/3'], label='HTTP/3', **styles['HTTP/3'])
ax.set_xlabel('Bandwidth (Mbps)')
ax.set_ylabel('Throughput (Mbps)')
ax.set_title('Throughput vs Bandwidth')
ax.set_yscale('log')  # Use log scale to make HTTP/3 visible
ax.legend()
ax.grid(True, alpha=0.3)

# 2. Page Load Time vs Bandwidth
ax = axes[0, 1]
ax.plot(h1['bandwidth'], h1['onload_s'], color=colors['HTTP/1.1'], label='HTTP/1.1', **styles['HTTP/1.1'])
ax.plot(h2['bandwidth'], h2['onload_s'], color=colors['HTTP/2'], label='HTTP/2', **styles['HTTP/2'])
ax.plot(h3['bandwidth'], h3['onload_s'], color=colors['HTTP/3'], label='HTTP/3', **styles['HTTP/3'])
ax.set_xlabel('Bandwidth (Mbps)')
ax.set_ylabel('Page Load Time (s)')
ax.set_title('Page Load Time vs Bandwidth')
ax.set_yscale('log')  # Use log scale to make differences visible
ax.legend()
ax.grid(True, alpha=0.3)

# 3. Response Time vs Bandwidth (only HTTP/2 and HTTP/3)
ax = axes[0, 2]
ax.plot(h2['bandwidth'], h2['avg_response_time_s'], color=colors['HTTP/2'], label='HTTP/2', **styles['HTTP/2'])
ax.plot(h3['bandwidth'], h3['avg_response_time_s'], color=colors['HTTP/3'], label='HTTP/3', **styles['HTTP/3'])
ax.text(0.05, 0.95, 'HTTP/1.1: N/A', transform=ax.transAxes, fontsize=10, 
        verticalalignment='top', bbox=dict(boxstyle='round', facecolor='lightgray', alpha=0.5))
ax.set_xlabel('Bandwidth (Mbps)')
ax.set_ylabel('Avg Response Time (s)')
ax.set_title('Response Time vs Bandwidth')
ax.legend()
ax.grid(True, alpha=0.3)

# 4. Jitter vs Bandwidth (only HTTP/2 and HTTP/3)
ax = axes[1, 0]
ax.plot(h2['bandwidth'], h2['jitter_s'], color=colors['HTTP/2'], label='HTTP/2', **styles['HTTP/2'])
ax.plot(h3['bandwidth'], h3['jitter_s'], color=colors['HTTP/3'], label='HTTP/3', **styles['HTTP/3'])
ax.text(0.05, 0.95, 'HTTP/1.1: N/A', transform=ax.transAxes, fontsize=10, 
        verticalalignment='top', bbox=dict(boxstyle='round', facecolor='lightgray', alpha=0.5))
ax.set_xlabel('Bandwidth (Mbps)')
ax.set_ylabel('Jitter (s)')
ax.set_title('Jitter vs Bandwidth')
ax.legend()
ax.grid(True, alpha=0.3)

# 5. Retransmissions vs Bandwidth (only HTTP/2 and HTTP/3)
ax = axes[1, 1]
ax.plot(h2['bandwidth'], h2['retransmissions'], color=colors['HTTP/2'], label='HTTP/2', **styles['HTTP/2'])
ax.plot(h3['bandwidth'], h3['retransmissions'], color=colors['HTTP/3'], label='HTTP/3', **styles['HTTP/3'])
ax.text(0.05, 0.95, 'HTTP/1.1: N/A', transform=ax.transAxes, fontsize=10, 
        verticalalignment='top', bbox=dict(boxstyle='round', facecolor='lightgray', alpha=0.5))
ax.set_xlabel('Bandwidth (Mbps)')
ax.set_ylabel('Retransmissions')
ax.set_title('Retransmissions vs Bandwidth')
ax.legend()
ax.grid(True, alpha=0.3)

# 6. Summary Panel
ax = axes[1, 2]
ax.axis('off')

# Calculate average metrics
h1_avg_throughput = h1['avg_throughput_Mbps'].mean()
h1_avg_plt = h1['onload_s'].mean()

h2_avg_throughput = h2['avg_throughput_Mbps'].mean()
h2_avg_plt = h2['onload_s'].mean()
h2_avg_response = h2['avg_response_time_s'].mean()
h2_avg_jitter = h2['jitter_s'].mean()
h2_avg_retx = h2['retransmissions'].mean()

h3_avg_throughput = h3['avg_throughput_Mbps'].mean()
h3_avg_plt = h3['onload_s'].mean()
h3_avg_response = h3['avg_response_time_s'].mean()
h3_avg_jitter = h3['jitter_s'].mean()
h3_avg_retx = h3['retransmissions'].mean()

# Create summary text
summary_text = f"""Performance Summary:

HTTP/1.1:
• Avg Throughput: {h1_avg_throughput:.2f} Mbps
• Avg PLT: {h1_avg_plt:.2f}s

HTTP/2:
• Avg Throughput: {h2_avg_throughput:.2f} Mbps
• Avg PLT: {h2_avg_plt:.2f}s
• Avg Response Time: {h2_avg_response:.2f}s
• Avg Jitter: {h2_avg_jitter:.3f}s
• Avg Retransmissions: {h2_avg_retx:.1f}

HTTP/3:
• Avg Throughput: {h3_avg_throughput:.2f} Mbps
• Avg PLT: {h3_avg_plt:.2f}s
• Avg Response Time: {h3_avg_response:.2f}s
• Avg Jitter: {h3_avg_jitter:.3f}s
• Avg Retransmissions: {h3_avg_retx:.1f}

Key Differences:
• HTTP/3 shows improved throughput
• HTTP/2 multiplexing reduces HoL blocking
• QPACK (HTTP/3) vs HPACK (HTTP/2) compression
• UDP-based QUIC (HTTP/3) vs TCP (HTTP/1.1,2)"""

ax.text(0.05, 0.95, summary_text, transform=ax.transAxes, fontsize=10,
        verticalalignment='top', fontfamily='monospace',
        bbox=dict(boxstyle='round,pad=0.5', facecolor='lightblue', alpha=0.1))

plt.tight_layout()

# Save the plot
plt.savefig('http1_vs_http2_vs_http3_multi.png', dpi=300, bbox_inches='tight')
plt.savefig('http1_vs_h2_vs_h3_multi.pdf', bbox_inches='tight')

print("Plots saved as:")
print("- http1_vs_http2_vs_http3_multi.png")
print("- http1_vs_h2_vs_h3_multi.pdf")

plt.show() 
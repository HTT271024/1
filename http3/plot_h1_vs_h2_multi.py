#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np

plt.style.use('seaborn-v0_8')
sns.set_palette('husl')

# Load data
h1 = pd.read_csv('../http1.1/corrected_bandwidth_test.csv')
h2 = pd.read_csv('../http2/summary_bw_h2_correct.csv')

# Normalize columns / types
h1 = h1.copy()
h2 = h2.copy()

h1['bandwidth_mbps'] = pd.to_numeric(h1['bandwidth'], errors='coerce')
h1['throughput_mbps'] = pd.to_numeric(h1['avg_throughput_Mbps'], errors='coerce')
h1['plt_s'] = pd.to_numeric(h1['onload_s'], errors='coerce')

h2['bandwidth_mbps'] = pd.to_numeric(h2['bandwidth_mbps'], errors='coerce')
h2['throughput_mbps'] = pd.to_numeric(h2['throughput_mbps'], errors='coerce')
h2['plt_s'] = pd.to_numeric(h2['plt_s'], errors='coerce')
h2['avg_delay_s'] = pd.to_numeric(h2['avg_delay_s'], errors='coerce')
h2['jitter_s'] = pd.to_numeric(h2['jitter_s'], errors='coerce')
h2['retx_count'] = pd.to_numeric(h2['retx_count'], errors='coerce')

# Sort by bandwidth
h1 = h1.sort_values('bandwidth_mbps')
h2 = h2.sort_values('bandwidth_mbps')

# Figure
fig, axes = plt.subplots(2, 3, figsize=(18, 10))
fig.suptitle('HTTP/1.1 vs HTTP/2 Performance Comparison', fontsize=16, fontweight='bold')

# 1) Bandwidth vs Throughput
ax = axes[0,0]
ax.plot(h1['bandwidth_mbps'], h1['throughput_mbps'], 'o-', linewidth=2, markersize=6, label='HTTP/1.1', color='darkorange')
ax.plot(h2['bandwidth_mbps'], h2['throughput_mbps'], 's-', linewidth=2, markersize=6, label='HTTP/2', color='royalblue')
ax.set_xlabel('Bandwidth (Mbps)')
ax.set_ylabel('Throughput (Mbps)')
ax.set_title('Bandwidth vs Throughput')
ax.grid(True, alpha=0.3)
ax.legend()

# 2) Bandwidth vs Page Load Time
ax = axes[0,1]
ax.plot(h1['bandwidth_mbps'], h1['plt_s'], 'o-', linewidth=2, markersize=6, label='HTTP/1.1', color='darkorange')
ax.plot(h2['bandwidth_mbps'], h2['plt_s'], 's-', linewidth=2, markersize=6, label='HTTP/2', color='royalblue')
ax.set_xlabel('Bandwidth (Mbps)')
ax.set_ylabel('Page Load Time (s)')
ax.set_title('Bandwidth vs Page Load Time')
ax.grid(True, alpha=0.3)
ax.legend()

# 3) Bandwidth vs Response Time (H1.1 unavailable)
ax = axes[0,2]
ax.plot(h2['bandwidth_mbps'], h2['avg_delay_s'], 's-', linewidth=2, markersize=6, label='HTTP/2', color='royalblue')
ax.set_xlabel('Bandwidth (Mbps)')
ax.set_ylabel('Average Response Time (s)')
ax.set_title('Bandwidth vs Response Time')
ax.grid(True, alpha=0.3)
ax.legend()
ax.text(0.05, 0.9, 'HTTP/1.1: N/A', transform=ax.transAxes, fontsize=9, color='gray')

# 4) Bandwidth vs Jitter (H1.1 unavailable)
ax = axes[1,0]
ax.plot(h2['bandwidth_mbps'], h2['jitter_s'], 's-', linewidth=2, markersize=6, label='HTTP/2', color='royalblue')
ax.set_xlabel('Bandwidth (Mbps)')
ax.set_ylabel('Jitter (s)')
ax.set_title('Bandwidth vs Jitter')
ax.grid(True, alpha=0.3)
ax.legend()
ax.text(0.05, 0.9, 'HTTP/1.1: N/A', transform=ax.transAxes, fontsize=9, color='gray')

# 5) Bandwidth vs Retransmissions (H1.1 unavailable)
ax = axes[1,1]
ax.plot(h2['bandwidth_mbps'], h2['retx_count'], 's-', linewidth=2, markersize=6, label='HTTP/2', color='royalblue')
ax.set_xlabel('Bandwidth (Mbps)')
ax.set_ylabel('Retransmission Count')
ax.set_title('Bandwidth vs Retransmissions')
ax.grid(True, alpha=0.3)
ax.legend()
ax.text(0.05, 0.9, 'HTTP/1.1: N/A', transform=ax.transAxes, fontsize=9, color='gray')

# 6) Summary panel
ax = axes[1,2]
ax.axis('off')

h1_thr_avg = h1['throughput_mbps'].mean()
h2_thr_avg = h2['throughput_mbps'].mean()
thr_change = ((h2_thr_avg - h1_thr_avg) / h1_thr_avg) * 100 if h1_thr_avg > 0 else np.nan

h1_plt_avg = h1['plt_s'].mean()
h2_plt_avg = h2['plt_s'].mean()
plt_change = ((h2_plt_avg - h1_plt_avg) / h1_plt_avg) * 100 if h1_plt_avg > 0 else np.nan

summary = f"""
HTTP/1.1 vs HTTP/2 Comparison:

Performance Metrics:
  HTTP/1.1 Avg Throughput: {h1_thr_avg:.2f} Mbps
  HTTP/2   Avg Throughput: {h2_thr_avg:.2f} Mbps
  Throughput Change: {thr_change:+.1f}%

  HTTP/1.1 Avg PLT: {h1_plt_avg:.2f} s
  HTTP/2   Avg PLT: {h2_plt_avg:.2f} s
  PLT Change: {plt_change:+.1f}%

Notes:
  - HTTP/1.1 dataset includes bandwidth→throughput & PLT only
  - Response time / jitter / retransmissions are not available for HTTP/1.1
"""
ax.text(0.02, 0.98, summary, va='top', ha='left', fontsize=10,
        family='monospace', bbox=dict(boxstyle='round', facecolor='#f5f5dc', alpha=0.9))

plt.tight_layout()
plt.savefig('http1_vs_http2_multi.png', dpi=300, bbox_inches='tight')
plt.savefig('http1_vs_http2_multi.pdf', bbox_inches='tight')
print('Saved http1_vs_http2_multi.png and http1_vs_http2_multi.pdf') 
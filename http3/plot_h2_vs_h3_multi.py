#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np

plt.style.use('seaborn-v0_8')
sns.set_palette('husl')

# Load data
h2 = pd.read_csv('../http2/summary_bw_h2_correct.csv')
h3 = pd.read_csv('summary_bw_h3.csv')

# Normalize columns / types
h2 = h2.copy()
h3 = h3.copy()

# HTTP/2
h2['bandwidth_mbps'] = pd.to_numeric(h2['bandwidth_mbps'], errors='coerce')
h2['throughput_mbps'] = pd.to_numeric(h2['throughput_mbps'], errors='coerce')
h2['plt_s'] = pd.to_numeric(h2['plt_s'], errors='coerce')
h2['avg_delay_s'] = pd.to_numeric(h2['avg_delay_s'], errors='coerce')
h2['jitter_s'] = pd.to_numeric(h2['jitter_s'], errors='coerce')
h2['retx_count'] = pd.to_numeric(h2['retx_count'], errors='coerce')

# HTTP/3
if 'bandwidth' in h3.columns:
    h3['bandwidth'] = h3['bandwidth'].astype(str).str.replace('Mbps','', regex=False)
    h3['bandwidth_mbps'] = pd.to_numeric(h3['bandwidth'], errors='coerce')
else:
    h3['bandwidth_mbps'] = pd.to_numeric(h3['bandwidth_mbps'], errors='coerce')

# Column names for throughput/PLT/RT
h3['avg_throughput_Mbps'] = pd.to_numeric(h3.get('avg_throughput_Mbps', h3.get('throughput_mbps')), errors='coerce')
h3['onload_s'] = pd.to_numeric(h3.get('onload_s', h3.get('plt_s')), errors='coerce')
h3['avg_delay_s'] = pd.to_numeric(h3['avg_delay_s'], errors='coerce')
h3['jitter_s'] = pd.to_numeric(h3['jitter_s'], errors='coerce')
h3['retx_count'] = pd.to_numeric(h3['retx_count'], errors='coerce')

# Sort by bandwidth
h2 = h2.sort_values('bandwidth_mbps')
h3 = h3.sort_values('bandwidth_mbps')

# Figure
fig, axes = plt.subplots(2, 3, figsize=(18, 10))
fig.suptitle('HTTP/2 vs HTTP/3 Performance Comparison', fontsize=16, fontweight='bold')

# 1) Bandwidth vs Throughput
ax = axes[0,0]
ax.plot(h2['bandwidth_mbps'], h2['throughput_mbps'], 'o-', linewidth=2, markersize=6, label='HTTP/2', color='royalblue')
ax.plot(h3['bandwidth_mbps'], h3['avg_throughput_Mbps'], 's-', linewidth=2, markersize=6, label='HTTP/3', color='crimson')
ax.set_xlabel('Bandwidth (Mbps)')
ax.set_ylabel('Throughput (Mbps)')
ax.set_title('Bandwidth vs Throughput')
ax.grid(True, alpha=0.3)
ax.legend()

# 2) Bandwidth vs Page Load Time
ax = axes[0,1]
ax.plot(h2['bandwidth_mbps'], h2['plt_s'], 'o-', linewidth=2, markersize=6, label='HTTP/2', color='royalblue')
ax.plot(h3['bandwidth_mbps'], h3['onload_s'], 's-', linewidth=2, markersize=6, label='HTTP/3', color='crimson')
ax.set_xlabel('Bandwidth (Mbps)')
ax.set_ylabel('Page Load Time (s)')
ax.set_title('Bandwidth vs Page Load Time')
ax.grid(True, alpha=0.3)
ax.legend()

# 3) Bandwidth vs Response Time
ax = axes[0,2]
ax.plot(h2['bandwidth_mbps'], h2['avg_delay_s'], 'o-', linewidth=2, markersize=6, label='HTTP/2', color='royalblue')
ax.plot(h3['bandwidth_mbps'], h3['avg_delay_s'], 's-', linewidth=2, markersize=6, label='HTTP/3', color='crimson')
ax.set_xlabel('Bandwidth (Mbps)')
ax.set_ylabel('Average Response Time (s)')
ax.set_title('Bandwidth vs Response Time')
ax.grid(True, alpha=0.3)
ax.legend()

# 4) Bandwidth vs Jitter
ax = axes[1,0]
ax.plot(h2['bandwidth_mbps'], h2['jitter_s'], 'o-', linewidth=2, markersize=6, label='HTTP/2', color='royalblue')
ax.plot(h3['bandwidth_mbps'], h3['jitter_s'], 's-', linewidth=2, markersize=6, label='HTTP/3', color='crimson')
ax.set_xlabel('Bandwidth (Mbps)')
ax.set_ylabel('Jitter (s)')
ax.set_title('Bandwidth vs Jitter')
ax.grid(True, alpha=0.3)
ax.legend()

# 5) Bandwidth vs Retransmissions
ax = axes[1,1]
ax.plot(h2['bandwidth_mbps'], h2['retx_count'], 'o-', linewidth=2, markersize=6, label='HTTP/2', color='royalblue')
ax.plot(h3['bandwidth_mbps'], h3['retx_count'], 's-', linewidth=2, markersize=6, label='HTTP/3', color='crimson')
ax.set_xlabel('Bandwidth (Mbps)')
ax.set_ylabel('Retransmission Count')
ax.set_title('Bandwidth vs Retransmissions')
ax.grid(True, alpha=0.3)
ax.legend()

# 6) Summary panel
ax = axes[1,2]
ax.axis('off')

h2_thr_avg = h2['throughput_mbps'].mean()
h3_thr_avg = h3['avg_throughput_Mbps'].mean()
thr_change = ((h3_thr_avg - h2_thr_avg) / h2_thr_avg) * 100 if h2_thr_avg > 0 else np.nan

h2_plt_avg = h2['plt_s'].mean()
h3_plt_avg = h3['onload_s'].mean()
plt_change = ((h3_plt_avg - h2_plt_avg) / h2_plt_avg) * 100 if h2_plt_avg > 0 else np.nan

summary = f"""
HTTP/2 vs HTTP/3 Comparison:

Performance Metrics:
  HTTP/2 Avg Throughput: {h2_thr_avg:.2f} Mbps
  HTTP/3 Avg Throughput: {h3_thr_avg:.2f} Mbps
  Throughput Change: {thr_change:+.1f}%

  HTTP/2 Avg PLT: {h2_plt_avg:.2f} s
  HTTP/3 Avg PLT: {h3_plt_avg:.2f} s
  PLT Change: {plt_change:+.1f}%

Key Differences:
  HTTP/2: TCP-based, HPACK compression
  HTTP/3: QUIC-based, QPACK compression
  HTTP/2: Stream multiplexing over TCP
  HTTP/3: Native stream multiplexing
"""
ax.text(0.02, 0.98, summary, va='top', ha='left', fontsize=10,
        family='monospace', bbox=dict(boxstyle='round', facecolor='#f5f5dc', alpha=0.9))

plt.tight_layout()
plt.savefig('http2_vs_http3_multi.png', dpi=300, bbox_inches='tight')
plt.savefig('http2_vs_http3_multi.pdf', bbox_inches='tight')
print("Saved http2_vs_http3_multi.png and http2_vs_http3_multi.pdf") 
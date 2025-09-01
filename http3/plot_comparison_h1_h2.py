#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np

plt.style.use('seaborn-v0_8')
sns.set_palette('husl')

# Load datasets
h1 = pd.read_csv('../http1.1/corrected_bandwidth_test.csv')
h2 = pd.read_csv('../http2/summary_bw_h2_correct.csv')

# Normalize columns
h1_bw = h1['bandwidth'].astype(float)
h1_thr = h1['avg_throughput_Mbps'].astype(float)
h1_plt = h1['onload_s'].astype(float)

h2_bw = h2['bandwidth_mbps'].astype(float)
h2_thr = h2['throughput_mbps'].astype(float)
h2_plt = h2['plt_s'].astype(float)

# Figure 1: Bandwidth vs Throughput
plt.figure(figsize=(10,6))
plt.plot(h1_bw, h1_thr, 'o-', linewidth=2.5, markersize=8, label='HTTP/1.1')
plt.plot(h2_bw, h2_thr, 's-', linewidth=2.5, markersize=8, label='HTTP/2')
plt.xlabel('Bandwidth (Mbps)')
plt.ylabel('Throughput (Mbps)')
plt.title('HTTP/1.1 vs HTTP/2: Bandwidth vs Throughput')
plt.grid(True, alpha=0.3)
plt.legend()
plt.tight_layout()
plt.savefig('h1_vs_h2_throughput.png', dpi=300, bbox_inches='tight')

# Figure 2: Bandwidth vs Page Load Time
plt.figure(figsize=(10,6))
plt.plot(h1_bw, h1_plt, 'o-', linewidth=2.5, markersize=8, label='HTTP/1.1')
plt.plot(h2_bw, h2_plt, 's-', linewidth=2.5, markersize=8, label='HTTP/2')
plt.xlabel('Bandwidth (Mbps)')
plt.ylabel('Page Load Time (s)')
plt.title('HTTP/1.1 vs HTTP/2: Bandwidth vs Page Load Time')
plt.grid(True, alpha=0.3)
plt.legend()
plt.tight_layout()
plt.savefig('h1_vs_h2_plt.png', dpi=300, bbox_inches='tight')

print("Saved h1_vs_h2_throughput.png and h1_vs_h2_plt.png") 
import matplotlib.pyplot as plt
import pandas as pd

# 读取数据
raw = pd.read_csv('delay_vs_bandwidth.csv')

bandwidths = raw['bandwidth'].str.replace('Mbps', '').astype(float)
delays = raw['avg_delay_ms_mean']
delay_stds = raw['avg_delay_ms_std']

plt.figure(figsize=(8,5))
plt.errorbar(bandwidths, delays, yerr=delay_stds, fmt='o-', color='green', capsize=5, label='mean±std')
plt.xscale('log')
plt.xticks(bandwidths, [f'{int(bw)}' for bw in bandwidths])
plt.xlabel('Bandwidth (Mbps, log scale)')
plt.ylabel('HTTP/1.1 Average Delay (ms)')
plt.title('HTTP/1.1 Average Delay vs Bandwidth (mean±std)')
plt.grid(True)
plt.tight_layout()
plt.savefig('avg_delay_vs_bandwidth.png', dpi=150)
plt.show()

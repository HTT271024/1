import matplotlib.pyplot as plt
import pandas as pd

# 读取数据
raw = pd.read_csv('delay_vs_bandwidth.csv')

# 读取完整输出，提取 complete_rate
with open('temp_output.txt', 'r') as f:
    lines = f.readlines()

import re
bandwidths = []
delays = []
delay_stds = []
completes = []
for line in lines[1:]:
    m = re.match(r'([0-9]+)Mbps,[^,]*,[^,]*,([^,]*),[^,]*,([^,]*),([^,]*),', line)
    if m:
        bw = int(m.group(1))
        complete = float(m.group(2))
        delay = float(m.group(3))
        delay_std = float(m.group(4))
        bandwidths.append(bw)
        completes.append(complete)
        delays.append(delay)
        delay_stds.append(delay_std)

plt.figure(figsize=(8,5))
plt.errorbar(bandwidths, delays, yerr=delay_stds, fmt='o-', color='green', capsize=5, label='mean±std')
for x, y, c in zip(bandwidths, delays, completes):
    plt.text(x, y, f'{c:.2f}', fontsize=10, ha='center', va='bottom')
plt.xscale('log')
plt.xticks(bandwidths, [str(bw) for bw in bandwidths])
plt.xlabel('Bandwidth (Mbps, log scale)')
plt.ylabel('HTTP/1.1 Average Delay (ms)')
plt.title('HTTP/1.1 Average Delay vs Bandwidth (mean±std, complete_rate)')
plt.grid(True)
plt.tight_layout()
plt.savefig('avg_delay_vs_bandwidth.png', dpi=150)
plt.show()

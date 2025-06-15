import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np

df = pd.read_csv('baseline_results.csv')

# 带宽转为数值型
df['bandwidth_num'] = df['bandwidth'].str.replace('Mbps', '').astype(float)

plt.style.use('default')
sns.set_palette("husl")

fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(18, 6))

# 1. 带宽 vs 吞吐量（不同延迟）
for delay in sorted(df['delay'].unique()):
    data = df[df['delay'] == delay].sort_values('bandwidth_num')
    ax1.plot(data['bandwidth_num'], data['throughput'], marker='o', label=f'Delay={delay}ms')
ax1.set_xlabel('Bandwidth (Mbps)')
ax1.set_ylabel('Throughput (kbps)')
ax1.set_title('Throughput vs Bandwidth')
ax1.set_xticks(df['bandwidth_num'].unique())
ax1.legend()
ax1.grid(True)

# 2. 延迟 vs 吞吐量（不同带宽）
for bw in sorted(df['bandwidth_num'].unique()):
    data = df[df['bandwidth_num'] == bw].sort_values('delay')
    ax2.plot(data['delay'], data['throughput'], marker='o', label=f'BW={int(bw)}Mbps')
ax2.set_xlabel('Delay (ms)')
ax2.set_ylabel('Throughput (kbps)')
ax2.set_title('Throughput vs Delay')
ax2.set_xticks(sorted(df['delay'].unique()))
ax2.legend()
ax2.grid(True)

# 3. 丢包率 vs 吞吐量（不同带宽）
for bw in sorted(df['bandwidth_num'].unique()):
    data = df[df['bandwidth_num'] == bw].sort_values('loss')
    ax3.plot(data['loss'] * 100, data['throughput'], marker='o', label=f'BW={int(bw)}Mbps')
ax3.set_xlabel('Loss Rate (%)')
ax3.set_ylabel('Throughput (kbps)')
ax3.set_title('Throughput vs Loss Rate')
ax3.set_xticks(sorted((df['loss'] * 100).unique()))
ax3.legend()
ax3.grid(True)

plt.tight_layout()
plt.savefig('http3_baseline_performance.png')
plt.close()

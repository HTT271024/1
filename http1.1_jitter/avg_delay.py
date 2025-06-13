import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

# 读取统计数据
df = pd.read_csv('jitter_stats.csv')
complete_rates = df['complete_rate']

# 读取每组 delays，计算 jitter
jitters = []
with open('delays_all.txt') as f:
    for line in f:
        if line.strip():
            delays = [float(x) for x in line.strip().split(',')]
            if len(delays) > 1:
                jitters.append(np.std(np.diff(delays)))
            else:
                jitters.append(0)

# 画 完成率 vs 抖动
plt.figure(figsize=(8,5))
plt.plot(jitters, complete_rates, marker='o')
plt.xlabel('Jitter (std of delay diffs, s)')
plt.ylabel('HTTP/1.1 Complete Rate')
plt.title('HTTP/1.1 Complete Rate vs Jitter')
plt.ylim(0, 1.05)
plt.grid(True)
plt.tight_layout()
plt.savefig('complete_rate_vs_jitter.png', dpi=150)
plt.show()

# 画 delay vs avg_delay
plt.plot(df['delay'], df['avg_delay'], marker='o')
plt.xlabel('Delay (ms)')
plt.ylabel('Average Delay (s)')
plt.savefig('delay_vs_avg_delay.png', dpi=150)
plt.show()

# 画 delay vs throughput
plt.plot(df['delay'], df['throughput'], marker='o')
plt.xlabel('Delay (ms)')
plt.ylabel('Throughput (Mbps)')
plt.savefig('delay_vs_throughput.png', dpi=150)
plt.show()


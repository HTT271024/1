import matplotlib.pyplot as plt
import pandas as pd

df = pd.read_csv('baseline_delay_clean.csv', header=None, names=['delay','avg_delay','complete_rate','throughput'])

if df['delay'].dtype == object:
    df['delay'] = df['delay'].str.replace('ms', '').astype(float)

plt.figure(figsize=(8,5))
plt.plot(df['delay'], df['throughput'], marker='o', color='blue')
plt.xlabel('Link Delay (ms)')
plt.ylabel('HTTP/1.1 Throughput (Mbps)')
plt.title('HTTP/1.1 Throughput vs Link Delay')
plt.grid(True)
plt.tight_layout()
plt.savefig('throughput_vs_link_delay.png', dpi=150)
plt.show()

import matplotlib.pyplot as plt
import pandas as pd

df = pd.read_csv('baseline_delay_clean.csv', header=None, names=['delay','avg_delay','complete_rate','throughput'])

# Remove 'ms' if present
if df['delay'].dtype == object:
    df['delay'] = df['delay'].str.replace('ms', '').astype(float)

plt.figure(figsize=(8,5))
plt.plot(df['delay'], df['avg_delay'], marker='o', color='red')
plt.xlabel('Baseline Link Delay (ms)')
plt.ylabel('HTTP/1.1 Average Delay (s)')
plt.title('HTTP/1.1 Average Delay vs Baseline Link Delay')
plt.grid(True)
plt.tight_layout()
plt.savefig('avg_delay_vs_baseline_delay.png', dpi=150)
plt.show()

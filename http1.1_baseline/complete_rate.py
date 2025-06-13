import matplotlib.pyplot as plt
import pandas as pd

df = pd.read_csv('baseline_delay_clean.csv', header=None, names=['delay','avg_delay','complete_rate','throughput'])

# Remove 'ms' if present
if df['delay'].dtype == object:
    df['delay'] = df['delay'].str.replace('ms', '').astype(float)

plt.figure(figsize=(8,5))
plt.plot(df['delay'], df['complete_rate'], marker='o', color='purple')
plt.xlabel('Link Delay (ms)')
plt.ylabel('HTTP/1.1 Complete Rate')
plt.title('HTTP/1.1 Complete Rate vs Link Delay')
plt.ylim(0, 1.05)
plt.grid(True)
plt.tight_layout()
plt.savefig('complete_rate_vs_link_delay.png', dpi=150)
plt.show()

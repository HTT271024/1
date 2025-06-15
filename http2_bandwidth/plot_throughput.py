import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("bandwidth_vs_throughput.csv")

# Convert bandwidth to numeric value (remove Mbps)
df['bandwidth'] = df['bandwidth'].str.replace('Mbps', '').astype(float)

plt.figure(figsize=(10, 6))
plt.plot(df["bandwidth"], df["throughput"], marker='o', linestyle='-', linewidth=2)
plt.xlabel("Link Bandwidth (Mbps)")
plt.ylabel("Achieved Throughput (Mbps)")
plt.title("HTTP/2 Throughput vs Link Bandwidth")
plt.grid(True)
plt.tight_layout()

# Add ideal line (45-degree line)
max_bw = max(df['bandwidth'].max(), df['throughput'].max())
plt.plot([0, max_bw], [0, max_bw], 'r--', label='Ideal Throughput')
plt.legend()

plt.savefig("http2_bandwidth_throughput.png")
plt.show()
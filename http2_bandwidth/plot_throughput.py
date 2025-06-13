import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("bandwidth_vs_throughput.csv")

# 将带宽转换为数值（去掉Mbps）
df['bandwidth'] = df['bandwidth'].str.replace('Mbps', '').astype(float)

plt.figure(figsize=(10, 6))
plt.plot(df["bandwidth"], df["throughput"], marker='o', linestyle='-', linewidth=2)
plt.xlabel("Link Bandwidth (Mbps)")
plt.ylabel("Achieved Throughput (Mbps)")
plt.title("HTTP/2 Throughput vs Link Bandwidth")
plt.grid(True)
plt.tight_layout()

# 添加理想线（45度线）
max_bw = max(df['bandwidth'].max(), df['throughput'].max())
plt.plot([0, max_bw], [0, max_bw], 'r--', label='Ideal Throughput')
plt.legend()

plt.savefig("http2_bandwidth_throughput.png")
plt.show()
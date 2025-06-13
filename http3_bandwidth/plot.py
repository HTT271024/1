import pandas as pd
import matplotlib.pyplot as plt

# 读取CSV文件
df = pd.read_csv('bandwidth_vs_throughput.csv')

plt.figure(figsize=(8,6))
plt.plot(df['bandwidth'], df['total_throughput'], marker='o')
plt.xlabel('Bandwidth')
plt.ylabel('Total Throughput (kbps)')
plt.title('Total Throughput vs Bandwidth')
plt.grid(True)
plt.tight_layout()
plt.savefig('http3_bandwidth_throughput.png')
plt.close()

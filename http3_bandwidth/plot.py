import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# 读取数据
df = pd.read_csv('bandwidth_vs_throughput.csv')

# 如果 throughput 太大，转换为 Mbps 单位
df['total_throughput_mbps'] = df['total_throughput'] / 1000

x = df['bandwidth']
y = df['total_throughput_mbps']

plt.figure(figsize=(9,6))
plt.plot(x, y, marker='o', label='HTTP/3 Throughput')

# 添加数据标签
for i in range(len(x)):
    plt.annotate(f"{y[i]:.2f}", (x[i], y[i]), textcoords="offset points", xytext=(0,10), ha='center')

# 添加最高点注释
max_idx = y.idxmax()
plt.annotate('🔼 Max Throughput', 
             xy=(x[max_idx], y[max_idx]), 
             xytext=(x[max_idx], y[max_idx] + 5),
             ha='center',
             arrowprops=dict(arrowstyle='->', color='black'))

# 均值参考线
plt.axhline(y.mean(), linestyle='--', color='gray', label=f'Mean = {y.mean():.2f} Mbps')

# 图表元素
plt.xlabel('Bandwidth (Mbps)')
plt.ylabel('Total Throughput (Mbps)')
plt.title('Total Throughput vs Bandwidth (HTTP/3)')
plt.grid(True)
plt.legend()
plt.tight_layout()

# 保存图像
plt.savefig('http3_bandwidth_throughput_annotated.png')
plt.show()

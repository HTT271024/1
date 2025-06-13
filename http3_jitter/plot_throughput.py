import pandas as pd
import matplotlib.pyplot as plt

# 读取CSV文件
df = pd.read_csv('jitter_vs_throughput.csv')

# 设置绘图样式
plt.style.use('default')

# 绘制抖动-吞吐量曲线
plt.figure(figsize=(10, 6))
plt.plot(df['jitter'], df['throughput'], marker='o', linestyle='-', color='blue')
plt.xlabel('Jitter (ms)')
plt.ylabel('Throughput (kbps)')
plt.title('HTTP/3 Throughput vs Network Jitter')
plt.grid(True)

# 保存图片
plt.savefig('http3_jitter_throughput.png')
plt.close() 
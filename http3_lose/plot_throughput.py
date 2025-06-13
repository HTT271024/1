import pandas as pd
import matplotlib.pyplot as plt

# 读取CSV文件
df = pd.read_csv('loss_vs_throughput.csv')

# 设置绘图样式
plt.style.use('default')

# 绘制丢包率-吞吐量曲线
plt.figure(figsize=(10, 6))
plt.plot(df['loss'] * 100, df['throughput'], marker='o', linestyle='-', color='blue')
plt.xlabel('Packet Loss Rate (%)')
plt.ylabel('Throughput (kbps)')
plt.title('HTTP/3 Throughput vs Packet Loss Rate')
plt.grid(True)

# 保存图片
plt.savefig('http3_loss_throughput.png')
plt.close() 
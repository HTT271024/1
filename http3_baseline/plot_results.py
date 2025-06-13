import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

# 读取CSV文件
df = pd.read_csv('baseline_results.csv')

# 设置绘图样式
plt.style.use('default')
sns.set_palette("husl")

# 创建三个子图
fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(18, 6))

# 1. 带宽vs吞吐量（不同延迟）
for delay in df['delay'].unique():
    data = df[df['delay'] == delay]
    ax1.plot(data['bandwidth'], data['throughput'], marker='o', label=f'Delay={delay}ms')
ax1.set_xlabel('Bandwidth')
ax1.set_ylabel('Throughput (kbps)')
ax1.set_title('Throughput vs Bandwidth')
ax1.legend()
ax1.grid(True)

# 2. 延迟vs吞吐量（不同带宽）
for bw in df['bandwidth'].unique():
    data = df[df['bandwidth'] == bw]
    ax2.plot(data['delay'], data['throughput'], marker='o', label=f'BW={bw}')
ax2.set_xlabel('Delay (ms)')
ax2.set_ylabel('Throughput (kbps)')
ax2.set_title('Throughput vs Delay')
ax2.legend()
ax2.grid(True)

# 3. 丢包率vs吞吐量（不同带宽）
for bw in df['bandwidth'].unique():
    data = df[df['bandwidth'] == bw]
    ax3.plot(data['loss'] * 100, data['throughput'], marker='o', label=f'BW={bw}')
ax3.set_xlabel('Loss Rate (%)')
ax3.set_ylabel('Throughput (kbps)')
ax3.set_title('Throughput vs Loss Rate')
ax3.legend()
ax3.grid(True)

# 调整布局
plt.tight_layout()

# 保存图片
plt.savefig('http3_baseline_performance.png')
plt.close() 
import matplotlib.pyplot as plt
import pandas as pd

plt.rcParams['font.sans-serif'] = ['DejaVu Sans']  # Use default English font
plt.rcParams['axes.unicode_minus'] = False

df = pd.read_csv('throughput.csv')

plt.figure(figsize=(8,5))
plt.plot(df['errorRate'], df['throughput'], marker='o')
plt.xlabel('Packet Loss Rate')
plt.ylabel('Throughput (Mbps)')
plt.title('ErrorRate vs Throughput')
plt.xticks([0, 0.01, 0.05, 0.1, 0.2, 0.3], ['0%', '1%', '5%', '10%', '20%', '30%'])
plt.grid(True)
plt.savefig('loss_vs_throughput.png', dpi=150)
plt.show()
# 图中显示 HTTP/1.1 在丢包率低于 5% 时仍能维持高吞吐量（≈7.3 Mbps），但在丢包率超过 10% 后，吞吐量迅速下滑至 1~2 Mbps，验证了其对丢包的敏感性和典型的 TCP HoL 阻塞效应。

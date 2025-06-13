import matplotlib.pyplot as plt
import pandas as pd

df = pd.read_csv('avg_delay.csv')
plt.figure(figsize=(8,5))
plt.plot(df['errorRate'], df['avg_delay'], marker='o', color='orange')
plt.xlabel('Packet Loss Rate')
plt.ylabel('Average Delay (s)')
plt.title('HTTP/1.1 Average Delay vs Packet Loss Rate')
plt.xticks([0, 0.01, 0.02, 0.05, 0.1, 0.2, 0.3], ['0%', '1%', '2%', '5%', '10%', '20%', '30%'])
plt.grid(True)
plt.savefig('avg_delay_vs_loss.png', dpi=150)
plt.show()

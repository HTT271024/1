import matplotlib.pyplot as plt
import pandas as pd

df = pd.read_csv('completion_rate.csv')
plt.figure(figsize=(8,5))
plt.bar([str(int(x*100))+'%' for x in df['errorRate']], df['completion_rate'], color='skyblue')
plt.xlabel('Packet Loss Rate')
plt.ylabel('Request Completion Rate')
plt.title('HTTP/1.1 Request Completion Rate vs Packet Loss Rate')
plt.ylim(0, 1.05)
plt.grid(axis='y')
plt.savefig('completion_rate_vs_loss.png', dpi=150)
plt.show()

import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

df = pd.read_csv("baseline_results.csv")

plt.figure(figsize=(8, 5))
sns.barplot(data=df, x='loss_rate', y='throughput', hue='protocol')
plt.xlabel('Packet Loss Rate')
plt.ylabel('Throughput (Mbps)')
plt.title('HTTP/1.1 vs HTTP/2 Baseline Performance')
plt.tight_layout()
plt.savefig("baseline_comparison.png")
plt.show()

import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("loss_vs_throughput.csv")

plt.figure(figsize=(8, 5))
plt.plot(df["loss"], df["throughput"], marker='o')
plt.xlabel("Packet Loss Rate")
plt.ylabel("Throughput (Mbps)")
plt.title("HTTP/2 Throughput vs Packet Loss Rate")
plt.grid(True)
plt.tight_layout()
plt.savefig("http2_throughput_loss.png")
plt.show()

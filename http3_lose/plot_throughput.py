import pandas as pd
import matplotlib.pyplot as plt

# Read CSV file
df = pd.read_csv('loss_vs_throughput.csv')

# Set plot style
plt.style.use('default')

# Plot loss rate vs throughput curve
plt.figure(figsize=(10, 6))
plt.plot(df['loss'] * 100, df['throughput'], marker='o', linestyle='-', color='blue')
plt.xlabel('Packet Loss Rate (%)')
plt.ylabel('Throughput (kbps)')
plt.title('HTTP/3 Throughput vs Packet Loss Rate')
plt.grid(True)

# Save figure
plt.savefig('http3_loss_throughput.png')
plt.close() 
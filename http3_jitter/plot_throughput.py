import pandas as pd
import matplotlib.pyplot as plt

# Read CSV file
df = pd.read_csv('jitter_vs_throughput.csv')

# Set plot style
plt.style.use('default')

# Plot jitter vs throughput curve
plt.figure(figsize=(10, 6))
plt.plot(df['jitter'], df['throughput'], marker='o', linestyle='-', color='blue')
plt.xlabel('Jitter (ms)')
plt.ylabel('Throughput (kbps)')
plt.title('HTTP/3 Throughput vs Network Jitter')
plt.grid(True)

# Save figure
plt.savefig('http3_jitter_throughput.png')
plt.close() 
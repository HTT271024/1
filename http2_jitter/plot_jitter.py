import pandas as pd
import matplotlib.pyplot as plt

# Read CSV file
df = pd.read_csv('jitter_vs_throughput.csv')

# Set plot style
plt.style.use('default')

# Plot jitter-throughput curve
plt.figure(figsize=(10, 6))
plt.plot(df['jitter'], df['throughput'], marker='o', linestyle='-', color='blue')
plt.xlabel('Jitter (ms)')
plt.ylabel('Throughput (Mbps)')
plt.title('HTTP/2 Jitter vs Throughput')
plt.grid(True)
plt.savefig('http2_jitter_throughput.png')
plt.close() 
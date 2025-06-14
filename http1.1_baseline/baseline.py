import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# Load the data
df = pd.read_csv('baseline_delay.csv')

# Remove 'ms' and convert to float
df['delay_ms'] = df['delay'].str.replace('ms', '', regex=False).astype(float)
df['avg_delay_ms'] = df['avg_delay'] * 1000  # Convert from seconds to ms

# Linear fit
z = np.polyfit(df['delay_ms'], df['avg_delay_ms'], 1)
p = np.poly1d(z)

# Plot
plt.figure(figsize=(8, 5))
plt.plot(df['delay_ms'], df['avg_delay_ms'], marker='o', color='blue', label='Avg Delay (ms)')
plt.plot(df['delay_ms'], p(df['delay_ms']), '--', color='gray', label='Linear Fit')

# Annotations
plt.annotate('Start point (lowest delay)', 
             xy=(df['delay_ms'].iloc[0], df['avg_delay_ms'].iloc[0]),
             xytext=(df['delay_ms'].iloc[0]+10, df['avg_delay_ms'].iloc[0]-30),
             arrowprops=dict(arrowstyle='->'))

plt.annotate('High latency bottleneck', 
             xy=(df['delay_ms'].iloc[-1], df['avg_delay_ms'].iloc[-1]),
             xytext=(df['delay_ms'].iloc[-1]-40, df['avg_delay_ms'].iloc[-1]+30),
             arrowprops=dict(arrowstyle='->'))

# Labels and title
plt.xlabel('Link Delay (ms)')
plt.ylabel('Avg HTTP Delay (ms)')
plt.title('HTTP/1.1 Average Delay vs Link Delay')
plt.grid(True)
plt.legend()
plt.tight_layout()

# Save
plt.savefig('baseline_delay_vs_linkdelay_annotated_en.png')
plt.show()

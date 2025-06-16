import pandas as pd
import matplotlib.pyplot as plt
import glob
import numpy as np
import re
import os

# Get absolute paths
script_dir = os.path.dirname(os.path.abspath(__file__))  # /ns-3-dev-new/scratch/http1.1
ns3_dir = os.path.dirname(os.path.dirname(script_dir))  # /ns-3-dev-new
results_dir = os.path.join(ns3_dir, 'experiment_results')
output_dir = os.path.join(ns3_dir, 'http1.1')  # 新的输出目录

# 确保输出目录存在
os.makedirs(output_dir, exist_ok=True)

print(f"Looking for results in: {results_dir}")
print(f"Output directory: {output_dir}")

# Read data
files = sorted(glob.glob(os.path.join(results_dir, 'sim_result_*.txt')))
data = []

for file in files:
    print(f"Processing file: {file}")
    with open(file, 'r') as f:
        content = f.read()
    # Get interval
    interval_match = re.search(r'Request Interval: ([0-9.]+) s', content)
    if interval_match:
        interval = float(interval_match.group(1))
    else:
        m = re.search(r'sim_result_([0-9.]+)\.txt', os.path.basename(file))
        if not m:
            print(f"Warning: filename {file} does not match expected pattern, skipping.")
            continue
        interval = float(m.group(1))

    try:
        # Extract metrics
        throughput = float(re.search(r'Average throughput of HTTP/1.1: ([0-9.]+) Mbps', content).group(1))
        delay = float(re.search(r'Average delay of HTTP/1.1: ([0-9.]+) s', content).group(1))
        page_load = float(re.search(r'Page Load Time \(onLoad\): ([0-9.]+) s', content).group(1))
        success_rate = float(re.search(r'responses received by the client is: (\d+)/\d+', content).group(1)) / 200.0
        
        data.append({
            'interval': interval,
            'throughput': throughput,
            'delay': delay,
            'page_load': page_load,
            'success_rate': success_rate
        })
        print(f"Successfully processed interval {interval}")
    except Exception as e:
        print(f"Error parsing {file}: {e}")
        continue

if not data:
    print("No data found! Please check if the experiment results exist.")
    exit(1)

# Convert to DataFrame
df = pd.DataFrame(data)
df = df.sort_values('interval')

# Create figure
plt.figure(figsize=(15, 10))

# ----------------------------
# 1. Throughput vs Interval
# ----------------------------
plt.subplot(2, 2, 1)
plt.plot(df['interval'], df['throughput'], 'bo-', label='Throughput')
plt.xlabel('Request Interval (s)')
plt.ylabel('Throughput (Mbps)')
plt.title('Throughput vs Request Interval\n(Single Connection)')
plt.grid(True)
plt.legend()

# Fit curve
z = np.polyfit(df['interval'], df['throughput'], 2)
p = np.poly1d(z)
plt.plot(df['interval'], p(df['interval']), "b--", alpha=0.3)

# Annotate max throughput
max_tp_idx = df['throughput'].idxmax()
max_tp_x = df.loc[max_tp_idx, 'interval']
max_tp_y = df.loc[max_tp_idx, 'throughput']
plt.annotate('⬆️ Max throughput',
             xy=(max_tp_x, max_tp_y),
             xytext=(max_tp_x - 0.05, max_tp_y + 0.5),
             arrowprops=dict(facecolor='blue', arrowstyle='->'),
             fontsize=10, color='blue')

# ----------------------------
# 2. Delay vs Interval
# ----------------------------
plt.subplot(2, 2, 2)
plt.plot(df['interval'], df['delay'], 'ro-', label='Average Delay')
plt.xlabel('Request Interval (s)')
plt.ylabel('Delay (s)')
plt.title('Delay vs Request Interval\n(Single Connection)')
plt.grid(True)
plt.legend()

# Fit curve
z = np.polyfit(df['interval'], df['delay'], 2)
p = np.poly1d(z)
plt.plot(df['interval'], p(df['interval']), "r--", alpha=0.3)

# Annotate min delay
min_d_idx = df['delay'].idxmin()
min_d_x = df.loc[min_d_idx, 'interval']
min_d_y = df.loc[min_d_idx, 'delay']
plt.annotate('✅ Best response',
             xy=(min_d_x, min_d_y),
             xytext=(min_d_x - 0.05, min_d_y + 0.04),
             arrowprops=dict(facecolor='green', arrowstyle='->'),
             fontsize=10, color='green')

# ----------------------------
# 3. Page Load Time
# ----------------------------
plt.subplot(2, 2, 3)
plt.plot(df['interval'], df['page_load'], 'go-', label='Page Load Time')
plt.xlabel('Request Interval (s)')
plt.ylabel('Time (s)')
plt.title('Page Load Time vs Request Interval\n(Single Connection)')
plt.grid(True)
plt.legend()

# ----------------------------
# 4. Success Rate
# ----------------------------
plt.subplot(2, 2, 4)
plt.plot(df['interval'], df['success_rate'] * 100, 'mo-', label='Success Rate')
plt.xlabel('Request Interval (s)')
plt.ylabel('Success Rate (%)')
plt.title('Success Rate vs Request Interval\n(Single Connection)')
plt.grid(True)
plt.legend()

plt.tight_layout()
output_file = os.path.join(output_dir, 'single_connection_results_annotated.png')
plt.savefig(output_file, dpi=300, bbox_inches='tight')
plt.close()

print(f"\nPlot saved to: {output_file}")

# Print summary
print("\nSingle Connection Results Summary:")
print("=" * 50)
print(f"Number of intervals tested: {len(df)}")
print("\nBest Performance:")
print(f"Highest Throughput: {df['throughput'].max():.2f} Mbps at interval {df.loc[df['throughput'].idxmax(), 'interval']:.3f}s")
print(f"Lowest Delay: {df['delay'].min():.4f} s at interval {df.loc[df['delay'].idxmin(), 'interval']:.3f}s")
print(f"Lowest Page Load Time: {df['page_load'].min():.4f} s at interval {df.loc[df['page_load'].idxmin(), 'interval']:.3f}s")
print(f"Highest Success Rate: {df['success_rate'].max()*100:.1f}% at interval {df.loc[df['success_rate'].idxmax(), 'interval']:.3f}s")
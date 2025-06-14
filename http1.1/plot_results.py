import pandas as pd
import matplotlib.pyplot as plt
import glob
import numpy as np
import re

# 读取数据
files = sorted(glob.glob('sim_result_*.txt'))
data = []

for file in files:
    with open(file, 'r') as f:
        content = f.read()
    # 获取 interval
    interval_match = re.search(r'Request Interval: ([0-9.]+) s', content)
    if interval_match:
        interval = float(interval_match.group(1))
    else:
        m = re.search(r'sim_result_([0-9.]+)\.txt', file)
        if not m:
            print(f"Warning: filename {file} does not match expected pattern, skipping.")
            continue
        interval = float(m.group(1))

    try:
        throughput = float(content.split('平均吞吐量: ')[1].split(' Mbps')[0])
        delay = float(content.split('平均延迟: ')[1].split(' s')[0])
        page_load = float(content.split('Page Load Time (onLoad): ')[1].split(' s')[0])
        data.append({
            'interval': interval,
            'throughput': throughput,
            'delay': delay,
            'page_load': page_load
        })
    except Exception as e:
        print(f"Error parsing {file}: {e}")
        continue

# 转为 DataFrame
df = pd.DataFrame(data)
df = df.sort_values('interval')

# 绘图
plt.figure(figsize=(15, 5))

# ----------------------------
# 1. Throughput vs Interval
# ----------------------------
plt.subplot(1, 3, 1)
plt.plot(df['interval'], df['throughput'], 'bo-', label='Throughput')
plt.xlabel('Request Interval (s)')
plt.ylabel('Throughput (Mbps)')
plt.title('Throughput vs Request Interval\n(Single Connection)')
plt.grid(True)
plt.legend()

# 拟合线（可选）
z = np.polyfit(df['interval'], df['throughput'], 2)
p = np.poly1d(z)
plt.plot(df['interval'], p(df['interval']), "b--", alpha=0.3)

# 注释最大吞吐
max_tp_idx = df['throughput'].idxmax()
max_tp_x = df.loc[max_tp_idx, 'interval']
max_tp_y = df.loc[max_tp_idx, 'throughput']
plt.annotate('⬆️ Max throughput',
             xy=(max_tp_x, max_tp_y),
             xytext=(max_tp_x - 0.05, max_tp_y + 0.5),
             arrowprops=dict(facecolor='blue', arrowstyle='->'),
             fontsize=10, color='blue')

# 注释最小吞吐
min_tp_idx = df['throughput'].idxmin()
min_tp_x = df.loc[min_tp_idx, 'interval']
min_tp_y = df.loc[min_tp_idx, 'throughput']
plt.annotate('❗HOL blocking severe',
             xy=(min_tp_x, min_tp_y),
             xytext=(min_tp_x + 0.02, min_tp_y + 1.0),
             arrowprops=dict(facecolor='red', arrowstyle='->'),
             fontsize=10, color='red')

# ----------------------------
# 2. Delay vs Interval
# ----------------------------
plt.subplot(1, 3, 2)
plt.plot(df['interval'], df['delay'], 'ro-', label='Average Delay')
plt.xlabel('Request Interval (s)')
plt.ylabel('Delay (s)')
plt.title('Delay vs Request Interval\n(Single Connection)')
plt.grid(True)
plt.legend()

# 拟合线
z = np.polyfit(df['interval'], df['delay'], 2)
p = np.poly1d(z)
plt.plot(df['interval'], p(df['interval']), "r--", alpha=0.3)

# 最大延迟
max_d_idx = df['delay'].idxmax()
max_d_x = df.loc[max_d_idx, 'interval']
max_d_y = df.loc[max_d_idx, 'delay']
plt.annotate('⛔ Delay peak',
             xy=(max_d_x, max_d_y),
             xytext=(max_d_x + 0.02, max_d_y + 0.05),
             arrowprops=dict(facecolor='red', arrowstyle='->'),
             fontsize=10, color='red')

# 最小延迟
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
plt.subplot(1, 3, 3)
plt.plot(df['interval'], df['page_load'], 'go-', label='Page Load Time')
plt.xlabel('Request Interval (s)')
plt.ylabel('Time (s)')
plt.title('Page Load Time vs Request Interval\n(Single Connection)')
plt.grid(True)
plt.legend()

plt.tight_layout()
plt.savefig('single_connection_results_annotated.png', dpi=300, bbox_inches='tight')
plt.close()

# 总结信息
print("\nSingle Connection Results Summary:")
print("=" * 50)
print(f"Number of intervals tested: {len(df)}")
print("\nBest Performance:")
print(f"Highest Throughput: {df['throughput'].max():.2f} Mbps at interval {df.loc[df['throughput'].idxmax(), 'interval']:.3f}s")
print(f"Lowest Delay: {df['delay'].min():.4f} s at interval {df.loc[df['delay'].idxmin(), 'interval']:.3f}s")
print(f"Lowest Page Load Time: {df['page_load'].min():.4f} s at interval {df.loc[df['page_load'].idxmin(), 'interval']:.3f}s")

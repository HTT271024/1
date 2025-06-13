import matplotlib.pyplot as plt
import pandas as pd
import glob

for csv_file in sorted(glob.glob('hol_timeline_loss_*.csv')):
    loss_str = csv_file.split('_')[-1].replace('.csv', '')
    df = pd.read_csv(csv_file)
    plt.figure(figsize=(10,6))
    for i, row in df.iterrows():
        plt.plot([row['send_time'], row['recv_time']], [row['request_id']]*2, marker='o', color='blue')
    plt.xlabel('Time (s)')
    plt.ylabel('Request ID')
    plt.title(f'HTTP/1.1 HoL Blocking Effect Timeline (loss={loss_str})')
    plt.grid(True)
    out_png = f'hol_effect_timeline_{loss_str}.png'
    plt.savefig(out_png, dpi=150)
    plt.close()
    print(f'Saved {out_png}')

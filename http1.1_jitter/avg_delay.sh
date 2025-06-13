#!/bin/bash

# 运行多组实验，收集 delays
echo "delay,avg_delay,complete_rate,throughput" > jitter_stats.csv
rm -f delays_all.txt

for d in 1ms 10ms 50ms 100ms
do
    output=$(../../ns3 run "scratch/http1.1_jitter/jitter --delay=$d" 2>&1)
    echo "$output"
    # 只提取统计数据行（以数字开头的行）
    stats_line=$(echo "$output" | grep -E '^[0-9]+ms,')
    echo "$stats_line" >> jitter_stats.csv
    delays_line=$(echo "$output" | grep '^delays,')
    delays=$(echo "$delays_line" | sed 's/^delays,//')
    echo "$delays" >> delays_all.txt
done

# 调用 Python 脚本自动分析并画图
python3 avg_delay.py



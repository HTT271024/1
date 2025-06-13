#!/bin/bash

# 创建CSV文件并写入表头
echo "jitter,throughput" > jitter_vs_throughput.csv

# 定义要测试的抖动值（毫秒）
jitters=(0 1 5 10 20 50)

# 循环测试每个抖动值
for jitter in "${jitters[@]}"
do
    echo "Testing with jitter: $jitter ms"
    # 运行仿真并提取吞吐量数据
    output=$(../../ns3 run "scratch/http3_jitter/jitter --jitter=$jitter")
    throughput=$(echo $output | grep -oP '吞吐量=\K[0-9\.]+' | head -n 1)
    # 将数据写入CSV文件
    echo "$jitter,$throughput" >> jitter_vs_throughput.csv
done

echo "CSV file has been generated: jitter_vs_throughput.csv" 
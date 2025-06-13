#!/bin/bash

# 创建CSV文件并写入表头
echo "loss,throughput" > loss_vs_throughput.csv

# 定义要测试的丢包率数组
loss_rates=(0.01 0.02 0.05 0.1 0.2 0.3)

# 循环测试每个丢包率
for loss in "${loss_rates[@]}"
do
    echo "Testing loss rate: $loss"
    # 运行仿真并提取吞吐量数据
    output=$(../../ns3 run "scratch/http3_lose/lose --errorRate=$loss")
    throughput=$(echo $output | grep -oP '吞吐量=\K[0-9\.]+' | head -n 1)
    # 将数据写入CSV文件
    echo "$loss,$throughput" >> loss_vs_throughput.csv
done

echo "CSV file has been generated: loss_vs_throughput.csv" 
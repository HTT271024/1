#!/bin/bash

# 创建CSV文件并写入表头
echo "bandwidth,delay,loss,throughput" > baseline_results.csv

# 定义要测试的网络条件
bandwidths=("1Mbps" "5Mbps" "10Mbps" "20Mbps" "50Mbps")
delays=(10 50 100 200)
losses=(0 0.01 0.05 0.1)

# 循环测试每个网络条件
for bw in "${bandwidths[@]}"
do
    for delay in "${delays[@]}"
    do
        for loss in "${losses[@]}"
        do
            echo "Testing with bandwidth=$bw, delay=${delay}ms, loss=$loss"
            # 运行仿真并提取吞吐量数据
            output=$(./ns3 run "scratch/http3_baseline/baseline --bandwidth=$bw --delay=$delay --loss=$loss")
            throughput=$(echo "$output" | grep "总接收吞吐量:" | grep -oP '\d+\.\d+')
            # 将数据写入CSV文件
            echo "$bw,$delay,$loss,$throughput" >> baseline_results.csv
        done
    done
done

echo "CSV file has been generated: baseline_results.csv" 
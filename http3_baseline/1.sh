#!/bin/bash

# 生成结果文件
echo "bandwidth,delay,loss,throughput" > baseline_results.csv

# 参数设置
bandwidths=("1Mbps" "5Mbps" "10Mbps" "20Mbps" "50Mbps")
delays=(10 50 100 200)
losses=(0 0.01 0.05 0.1)

for bw in "${bandwidths[@]}"
do
    for delay in "${delays[@]}"
    do
        for loss in "${losses[@]}"
        do
            echo "Running: bandwidth=$bw, delay=${delay}ms, loss=$loss"
            output=$(../../ns3 run "scratch/http3_baseline/baseline --bandwidth=$bw --delay=$delay --loss=$loss")
            throughput=$(echo "$output" | grep "总接收吞吐量:" | grep -oP '\d+\\.\\d+')
            echo "$bw,$delay,$loss,$throughput" >> baseline_results.csv
        done
    done
done

echo "CSV file has been generated: baseline_results.csv"
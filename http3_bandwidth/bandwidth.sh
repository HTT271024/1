#!/bin/bash

# 进入脚本所在目录
cd "$(dirname "$0")"

# 创建CSV文件并写入表头
echo "bandwidth,total_throughput" > bandwidth_vs_throughput.csv

# 定义要测试的带宽
bandwidths=("1Mbps" "5Mbps" "10Mbps" "20Mbps" "50Mbps")

for bw in "${bandwidths[@]}"
do
    echo "Testing with bandwidth=$bw"
    # 注意：../../ns3 路径指向 ns-3 根目录下的 ns3 可执行文件
    output=$(cd ../../ && ./ns3 run "scratch/http3_bandwidth/bandwidth --bandwidth=$bw")
    throughput=$(echo "$output" | grep "total_throughput:" | grep -oP '\d+\.\d+')
    echo "$bw,$throughput" >> bandwidth_vs_throughput.csv
done

echo "CSV file has been generated: bandwidth_vs_throughput.csv"
#!/bin/bash

# 创建CSV文件并写入表头
echo "bandwidth,throughput" > bandwidth_vs_throughput.csv

# 定义要测试的带宽数组
bandwidths=("1Mbps" "5Mbps" "10Mbps" "20Mbps" "50Mbps" "100Mbps")

# 循环测试每个带宽
for bw in "${bandwidths[@]}"
do
    echo "Testing bandwidth: $bw"
    # 运行仿真并提取吞吐量数据
    output=$(../../ns3 run "scratch/http2_bandwidth/bandwidth --bandwidth=$bw")
    throughput=$(echo $output | grep -oP 'throughput=\K[0-9\.]+')
    # 将数据写入CSV文件
    echo "$bw,$throughput" >> bandwidth_vs_throughput.csv
done

echo "CSV file has been generated: bandwidth_vs_throughput.csv"
#!/bin/bash

# 创建目录（如果不存在）
mkdir -p scratch/http2_baseline

# 创建CSV文件并写入表头
echo "protocol,loss_rate,throughput" > scratch/http2_baseline/baseline_results.csv

# 定义要测试的丢包率
loss_rates=("0.0" "0.01")

# 测试 HTTP/1.1
for loss in "${loss_rates[@]}"
do
    echo "Testing HTTP/1.1 with loss rate: $loss"
    output=$(../../ns3 run "scratch/http2_baseline/baseline --errorRate=$loss --isHttp2=false")
    throughput=$(echo $output | grep -oP 'throughput=\K[0-9\.]+')
    echo "HTTP/1.1,$loss,$throughput" >> scratch/http2_baseline/baseline_results.csv
done

# 测试 HTTP/2
for loss in "${loss_rates[@]}"
do
    echo "Testing HTTP/2 with loss rate: $loss"
    output=$(../../ns3 run "scratch/http2_baseline/baseline --errorRate=$loss --isHttp2=true")
    throughput=$(echo $output | grep -oP 'throughput=\K[0-9\.]+')
    echo "HTTP/2,$loss,$throughput" >> scratch/http2_baseline/baseline_results.csv
done

echo "CSV file has been generated: scratch/http2_baseline/baseline_results.csv" 
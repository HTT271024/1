#!/bin/bash

# 创建结果目录
echo "创建结果目录..."
mkdir -p ./results

# 测试不同的延迟值 (0-50ms)
echo "测试延迟值..."
for delay in 0 5 10 20 30 40 50; do
    echo "测试延迟: ${delay}ms"
    ../../ns3 run "scratch/http1.1_jitter/jitter --delay=$delay" > ./results/delay_${delay}ms.txt
done

# 测试不同的带宽 (1-10Mbps)
echo "测试带宽..."
for rate in 1 2 3 4 5 6 7 8 9 10; do
    echo "测试带宽: ${rate}Mbps"
    ../../ns3 run "scratch/http1.1_jitter/jitter --dataRate=${rate}Mbps" > ./results/rate_${rate}Mbps.txt
done

# 测试不同的丢包率 (0-5%)
echo "测试丢包率..."
for loss in 0 0.001 0.005 0.01 0.02 0.03 0.04 0.05; do
    echo "测试丢包率: ${loss}"
    ../../ns3 run "scratch/http1.1_jitter/jitter --errorRate=$loss" > ./results/loss_${loss}.txt
done

# 测试不同的并发连接数 (1-10)
echo "测试并发连接数..."
for conn in 1 2 3 4 5 6 7 8 9 10; do
    echo "测试并发连接数: ${conn}"
    ../../ns3 run "scratch/http1.1_jitter/jitter --nConnections=$conn" > ./results/conn_${conn}.txt
done

echo "所有测试完成，结果保存在 ./results 目录下"
ls -l ./results/ 
#!/bin/bash

# 切换到ns-3目录
cd /home/ekko/ns-3-dev-new

# 创建结果目录
echo "创建结果目录..."
mkdir -p scratch/new/results

# 测试不同的延迟值 (0-50ms)
echo "测试延迟值..."
for delay in 0 5 10 20 30 40 50; do
    echo "测试延迟: ${delay}ms"
    ./ns3 run "scratch/new/jitter --delay=$delay" > scratch/new/results/delay_${delay}ms.txt
done

# 测试不同的带宽 (1-10Mbps)
echo "测试带宽..."
for rate in 1 2 3 4 5 6 7 8 9 10; do
    echo "测试带宽: ${rate}Mbps"
    ./ns3 run "scratch/new/jitter --dataRate=${rate}Mbps" > scratch/new/results/rate_${rate}Mbps.txt
done

# 测试不同的丢包率 (0-5%)
echo "测试丢包率..."
for loss in 0 0.001 0.005 0.01 0.02 0.03 0.04 0.05; do
    echo "测试丢包率: ${loss}"
    ./ns3 run "scratch/new/jitter --errorRate=$loss" > scratch/new/results/loss_${loss}.txt
done

# 测试不同的并发连接数 (1-10)
echo "测试并发连接数..."
for conn in 1 2 3 4 5 6 7 8 9 10; do
    echo "测试并发连接数: ${conn}"
    ./ns3 run "scratch/new/jitter --nConnections=$conn" > scratch/new/results/conn_${conn}.txt
done

echo "所有测试完成，结果保存在 scratch/new/results 目录下"
ls -l scratch/new/results/ 
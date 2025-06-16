#!/bin/bash

# 输出表头
echo "bandwidth,avg_delay_ms_mean,avg_delay_ms_std" > delay_vs_bandwidth.csv

for bw in 1Mbps 2Mbps 3Mbps 5Mbps 8Mbps 10Mbps 15Mbps 20Mbps 30Mbps 50Mbps 100Mbps; do
  # 运行一次 bandwidth 程序，并将输出保存到临时文件
  ../../ns3 run "scratch/http1.1_bandwidth/bandwidth --dataRate=$bw" > temp_output.txt

  awk -F',' '/^[0-9]+Mbps/ {print $0}' temp_output.txt >> delay_vs_bandwidth.csv

  # 不再删除 temp_output.txt
done

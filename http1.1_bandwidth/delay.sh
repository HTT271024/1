#!/bin/bash

# 输出表头
echo "bandwidth,avg_delay_ms_mean,avg_delay_ms_std" > delay_vs_bandwidth.csv

# 运行一次 bandwidth 程序，并将输出保存到临时文件
../../ns3 run "scratch/http1.1_bandwidth/bandwidth" > temp_output.txt

# 提取每种带宽的均值和标准差
awk -F',' 'NR>1{print $1 "," $6 "," $7}' temp_output.txt >> delay_vs_bandwidth.csv

# 不再删除 temp_output.txt

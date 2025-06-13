#!/bin/bash

echo "errorRate,throughput" > throughput.csv

for loss in 0 0.01 0.02 0.05 0.1 0.2 0.3
do
    sum=0
    runs=30
    for i in $(seq 1 $runs)
    do
        output=$(../../ns3 run "scratch/http1.1_lose/lose --nRequests=200 --errorRate=$loss" 2>&1)
        throughput=$(echo "$output" | grep "HTTP/1.1 平均吞吐量" | grep -oP '[0-9.]+(?= Mbps)')
        if [ -n "$throughput" ]; then
        sum=$(awk "BEGIN{print $sum+$throughput}")
        fi
    done
    avg=$(awk "BEGIN{print $sum/$runs}")
    echo "$loss,$avg"
    echo "$loss,$avg" >> throughput.csv
done

# run_loss_vs_throughput.sh 示例（伪代码）
# for loss in 0 0.01 0.02 0.05 0.1 0.2 0.3; do
#   total=0
#   for i in {1..5}; do
#     result=$(./ns3 run "scratch/http1.1-lose --errorRate=$loss")
#     throughput=$(parse_throughput "$result")
#     total=$(echo "$total + $throughput" | bc -l)
#   done
#   avg=$(echo "$total / 5" | bc -l)
#   echo "$loss,$avg"
# done

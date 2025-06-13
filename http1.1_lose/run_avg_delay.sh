#!/bin/bash

echo "errorRate,avg_delay" > avg_delay.csv

for loss in 0 0.01 0.02 0.05 0.1 0.2 0.3
do
    sum=0
    runs=30
    for i in $(seq 1 $runs)
    do
        output=$(../../ns3 run "scratch/http1.1_lose/lose --nRequests=50 --errorRate=$loss" 2>&1)
        delay=$(echo "$output" | grep "HTTP/1.1 平均延迟" | grep -oP '[0-9.]+(?= s)')
        if [ -n "$delay" ]; then
        sum=$(awk "BEGIN{print $sum+$delay}")
        fi
    done
    avg=$(awk "BEGIN{print $sum/$runs}")
    echo "$loss,$avg"
    echo "$loss,$avg" >> avg_delay.csv
done

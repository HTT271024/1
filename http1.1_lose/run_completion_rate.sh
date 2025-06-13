#!/bin/bash

echo "errorRate,completion_rate" > completion_rate.csv

for loss in 0 0.01 0.02 0.05 0.1 0.2 0.3
do
    sum=0
    runs=30
    for i in $(seq 1 $runs)
    do
        output=$(../../ns3 run "scratch/http1.1_lose/lose --nRequests=50 --errorRate=$loss" 2>&1)
        completion=$(echo "$output" | grep "客户端共收到响应数" | grep -oP '[0-9]+/(?=50)' | head -1)
        if [ -n "$completion" ]; then
            got=$(echo $completion | cut -d'/' -f1)
            sum=$((sum+got))
        fi
    done
    avg=$(awk "BEGIN{printf \"%.4f\", $sum/($runs*50)}")
    echo "$loss,$avg"
    echo "$loss,$avg" >> completion_rate.csv
done

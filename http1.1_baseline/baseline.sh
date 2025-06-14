#!/bin/bash

echo "delay,avg_delay,complete_rate,throughput" > baseline_delay.csv

for d in 1ms 10ms 50ms 100ms
do
    ../../ns3 run "scratch/http1.1_baseline/baseline --delay=$d" | tail -n 1 | awk -F',' '{print $1 "," $10 "," $11 "," $12}' >> baseline_delay.csv
done

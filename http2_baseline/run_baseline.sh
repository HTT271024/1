#!/bin/bash

# Create directory (if it doesn't exist)
mkdir -p scratch/http2_baseline

# Create CSV file and write header
echo "protocol,loss_rate,throughput" > scratch/http2_baseline/baseline_results.csv

# Define loss rates to test
loss_rates=("0.0" "0.01")

# Test HTTP/1.1
for loss in "${loss_rates[@]}"
do
    echo "Testing HTTP/1.1 with loss rate: $loss"
    output=$(../../ns3 run "scratch/http2_baseline/baseline --errorRate=$loss --isHttp2=false")
    throughput=$(echo $output | grep -oP 'throughput=\K[0-9\.]+')
    echo "HTTP/1.1,$loss,$throughput" >> scratch/http2_baseline/baseline_results.csv
done

# Test HTTP/2
for loss in "${loss_rates[@]}"
do
    echo "Testing HTTP/2 with loss rate: $loss"
    output=$(../../ns3 run "scratch/http2_baseline/baseline --errorRate=$loss --isHttp2=true")
    throughput=$(echo $output | grep -oP 'throughput=\K[0-9\.]+')
    echo "HTTP/2,$loss,$throughput" >> scratch/http2_baseline/baseline_results.csv
done

echo "CSV file has been generated: scratch/http2_baseline/baseline_results.csv" 
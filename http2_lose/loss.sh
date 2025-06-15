#!/bin/bash

mkdir -p scratch/http2_lose

# Create CSV file and write header
echo "loss,throughput" > loss_vs_throughput.csv

# Define array of loss rates to test
loss_rates=(0.02 0.03 0.1 0.2 0.3)

# Loop through each loss rate
for loss in "${loss_rates[@]}"
do
    echo "Testing loss rate: $loss"
    # Run simulation and extract throughput data
    output=$(../../ns3 run "scratch/http2_lose/lose --errorRate=$loss")
    throughput=$(echo $output | grep -oP 'throughput=\K[0-9\.]+')
    # Write data to CSV file
    echo "$loss,$throughput" >> loss_vs_throughput.csv
done

echo "CSV file has been generated: loss_vs_throughput.csv"

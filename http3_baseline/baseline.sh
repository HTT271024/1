#!/bin/bash

# Create CSV file and write header
echo "bandwidth,delay,loss,throughput" > baseline_results.csv

# Define network conditions to test
bandwidths=("1Mbps" "5Mbps" "10Mbps" "20Mbps" "50Mbps")
delays=(10 50 100 200)
losses=(0 0.01 0.05 0.1)

# Loop through each network condition
for bw in "${bandwidths[@]}"
do
    for delay in "${delays[@]}"
    do
        for loss in "${losses[@]}"
        do
            echo "Testing with bandwidth=$bw, delay=${delay}ms, loss=$loss"
            # Run simulation and extract throughput data
            output=$(../../ns3 run "scratch/http3_baseline/baseline --bandwidth=$bw --delay=$delay --loss=$loss")
            throughput=$(echo "$output" | grep -E "total throughput:" | grep -oP '\d+(\.\d+)?')
            # Write data to CSV file
            echo "$bw,$delay,$loss,$throughput" >> baseline_results.csv
        done
    done
done

echo "CSV file has been generated: baseline_results.csv" 
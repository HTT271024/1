#!/bin/bash

# Create CSV file and write header
echo "bandwidth,throughput" > bandwidth_vs_throughput.csv

# Define bandwidth array for testing
bandwidths=("1Mbps" "5Mbps" "10Mbps" "20Mbps" "50Mbps" "100Mbps")

# Loop through each bandwidth for testing
for bw in "${bandwidths[@]}"
do
    echo "Testing bandwidth: $bw"
    # Run simulation and extract throughput data
    output=$(../../ns3 run "scratch/http2_bandwidth/bandwidth --bandwidth=$bw")
    throughput=$(echo $output | grep -oP 'throughput=\K[0-9\.]+')
    # Write data to CSV file
    echo "$bw,$throughput" >> bandwidth_vs_throughput.csv
done

echo "CSV file has been generated: bandwidth_vs_throughput.csv"
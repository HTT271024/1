#!/bin/bash

# Create CSV file and write header
echo "jitter,throughput" > jitter_vs_throughput.csv

# Define jitter values to test (ms)
jitters=(0 1 5 10 20 50)

# Loop through each jitter value
for jitter in "${jitters[@]}"
do
    echo "Testing with jitter: $jitter ms"
    # Run simulation and extract throughput data
    output=$(../../ns3 run "scratch/http3_jitter/jitter --jitter=$jitter")
    throughput=$(echo $output | grep -oP 'throughput=\K[0-9\.]+' | head -n 1)
    # Write data to CSV file
    echo "$jitter,$throughput" >> jitter_vs_throughput.csv
done

echo "CSV file has been generated: jitter_vs_throughput.csv" 
#!/bin/bash

mkdir -p scratch/http2_jitter

# Create CSV file and write header
echo "jitter,throughput" > jitter_vs_throughput.csv

# Define jitter values to test (in milliseconds)
jitters=("0" "1" "5" "10" "20" "50")

# Loop through each jitter value
for jitter in "${jitters[@]}"
do
    echo "Testing with jitter: $jitter ms"
    output=$(../../ns3 run "scratch/http2_jitter/jitter --jitter=$jitter")
    throughput=$(echo $output | grep -oP 'throughput=\K[0-9\.]+')
    echo "$jitter,$throughput" >> jitter_vs_throughput.csv
done

echo "CSV file has been generated: jitter_vs_throughput.csv"


cd "$(dirname "$0")"

echo "bandwidth,total_throughput" > bandwidth_vs_throughput.csv

# Define the bandwidth to be tested
bandwidths=("1Mbps" "5Mbps" "10Mbps" "20Mbps" "50Mbps")

for bw in "${bandwidths[@]}"
do
    echo "Testing with bandwidth=$bw"
    # Path pointing
    output=$(cd ../../ && ./ns3 run "scratch/http3_bandwidth/bandwidth --bandwidth=$bw")
    throughput=$(echo "$output" | grep "total_throughput:" | grep -oP '\d+\.\d+')
    echo "$bw,$throughput" >> bandwidth_vs_throughput.csv
done

echo "CSV file has been generated: bandwidth_vs_throughput.csv"
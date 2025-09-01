#!/usr/bin/env bash
set -euo pipefail

NS3_ROOT="$HOME/ns-3-dev-new"
APP="scratch/http1.1/sim"
OUT="$NS3_ROOT/scratch/http1.1"

# Create output directory
mkdir -p "$OUT"

# Define parameters
LATENCIES=("0ms" "50ms" "100ms" "200ms")
BANDWIDTH="10Mbps"
LOSS="0.01"
N_REQUESTS="200"
RESP_SIZE="102400"
REQ_SIZE="1024"
INTERVAL="0.05"
N_CONNECTIONS="1"

# Build ns-3
cd "$NS3_ROOT"
./ns3 build

# Create CSV file and write header
CSV="$OUT/latency_sweep.csv"
echo "latency,bandwidth,loss,avg_delay_s,avg_throughput_Mbps,onload_s,retx_count,retx_rate_per_s,jitter_s,hol_events,hol_time_s" > "$CSV"

# Loop through each latency
for DELAY in "${LATENCIES[@]}"; do
    echo "Testing latency: $DELAY"
    
    # Run simulation
    output=$(./ns3 run "$APP --delay=$DELAY --dataRate=$BANDWIDTH --errorRate=$LOSS --nRequests=$N_REQUESTS --respSize=$RESP_SIZE --reqSize=$REQ_SIZE --interval=$INTERVAL --nConnections=$N_CONNECTIONS" 2>/dev/null)
    
    # Extract metrics using grep and awk
    avg_delay=$(echo "$output" | grep -oP 'Average delay of HTTP/1.1: \K[0-9.]+')
    avg_throughput=$(echo "$output" | grep -oP 'Average throughput of HTTP/1.1: \K[0-9.]+')
    onload=$(echo "$output" | grep -oP 'Page Load Time \(onLoad\): \K[0-9.]+')
    retx_count=$(echo "$output" | grep -oP 'TCP retransmissions: \K[0-9]+')
    retx_rate=$(echo "$output" | grep -oP 'rate: \K[0-9.]+')
    jitter=$(echo "$output" | grep -oP 'RFC3550 jitter estimate: \K[0-9.]+')
    hol_events=$(echo "$output" | grep -oP 'HoL events: \K[0-9]+')
    hol_time=$(echo "$output" | grep -oP 'HoL blocked time: \K[0-9.]+')
    
    # 数据清理：确保所有字段都有有效值，缺失时用0填充
    avg_delay=${avg_delay:-0}
    avg_throughput=${avg_throughput:-0}
    onload=${onload:-0}
    retx_count=${retx_count:-0}
    retx_rate=${retx_rate:-0}
    jitter=${jitter:-0}
    hol_events=${hol_events:-0}
    hol_time=${hol_time:-0}
    
    # Write data to CSV
    echo "$DELAY,$BANDWIDTH,$LOSS,$avg_delay,$avg_throughput,$onload,$retx_count,$retx_rate,$jitter,$hol_events,$hol_time" >> "$CSV"
    
    echo "Completed: $DELAY"
done

echo "All latency sweep experiments completed. Results saved to: $CSV" 
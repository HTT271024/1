#!/usr/bin/env bash
set -euo pipefail

NS3_ROOT="$HOME/ns-3-dev-new"
APP_PATH="scratch/http2/http2"
OUTDIR="$NS3_ROOT/scratch/http2"

# Create output directory
mkdir -p "$OUTDIR"

# Define parameters
LOSS_RATES=("0.001" "0.005" "0.01" "0.02" "0.05")
BANDWIDTH="10Mbps"
LATENCY="5ms"
N_REQUESTS="200"
RESP_SIZE="102400"
REQ_SIZE="1024"
INTERVAL="0.05"
N_CONNECTIONS="1"
N_STREAMS="3"
FRAME_CHUNK="1200"
TICK_US="500"
HEADER_SIZE="200"
HPACK_RATIO="0.3"
CONN_WINDOW_MB="32"
STREAM_WINDOW_MB="32"

# Build ns-3
cd "$NS3_ROOT"
./ns3 build

# Create CSV file and write header
CSV="$OUTDIR/loss_sweep_h2.csv"
echo "loss_rate,bandwidth,latency,avg_delay_s,avg_throughput_Mbps,onload_s,retx_count,retx_rate_per_s,jitter_s,hol_events,hol_time_s,conn_hol_stall_s,conn_hol_ratio_percent,hpack_saved_bytes,hpack_compression_percent" > "$CSV"

# Loop through each loss rate
for LOSS in "${LOSS_RATES[@]}"; do
    echo "Testing loss rate: $LOSS"
    
    # Run simulation
    output=$(./ns3 run "$APP_PATH --dataRate=$BANDWIDTH --delay=$LATENCY --errorRate=$LOSS --nRequests=$N_REQUESTS --respSize=$RESP_SIZE --reqSize=$REQ_SIZE --interval=$INTERVAL --nConnections=$N_CONNECTIONS --nStreams=$N_STREAMS --frameChunk=$FRAME_CHUNK --tickUs=$TICK_US --headerSize=$HEADER_SIZE --hpackRatio=$HPACK_RATIO --connWindowMB=$CONN_WINDOW_MB --streamWindowMB=$STREAM_WINDOW_MB" 2>/dev/null)
    
    # Extract metrics using grep and awk
    avg_delay=$(echo "$output" | grep -oP 'Average delay of HTTP/2: \K[0-9.]+')
    avg_throughput=$(echo "$output" | grep -oP 'Downlink throughput: \K[0-9.]+')
    onload=$(echo "$output" | grep -oP 'Page Load Time \(onLoad\): \K[0-9.]+')
    retx_count=$(echo "$output" | grep -oP 'TCP retransmissions: \K[0-9]+')
    retx_rate=$(echo "$output" | grep -oP 'rate: \K[0-9.]+')
    jitter=$(echo "$output" | grep -oP 'RFC3550 jitter estimate: \K[0-9.]+')
    hol_events=$(echo "$output" | grep -oP 'HoL events: \K[0-9]+')
    hol_time=$(echo "$output" | grep -oP 'HoL blocked time: \K[0-9.]+')
    conn_hol_stall=$(echo "$output" | grep -oP 'TCP-level HoL stall time: \K[0-9.]+')
    conn_hol_ratio=$(echo "$output" | grep -oP 'stall ratio=\K[0-9.]+')
    hpack_saved=$(echo "$output" | grep -oP 'HPACK compression: saved \K[0-9]+')
    hpack_compression=$(echo "$output" | grep -oP '(\K[0-9.]+%)')
    
    # 数据清理：确保所有字段都有有效值，缺失时用0填充
    avg_delay=${avg_delay:-0}
    avg_throughput=${avg_throughput:-0}
    onload=${onload:-0}
    retx_count=${retx_count:-0}
    retx_rate=${retx_rate:-0}
    jitter=${jitter:-0}
    hol_events=${hol_events:-0}
    hol_time=${hol_time:-0}
    conn_hol_stall=${conn_hol_stall:-0}
    conn_hol_ratio=${conn_hol_ratio:-0}
    hpack_saved=${hpack_saved:-0}
    hpack_compression=${hpack_compression:-0}
    
    # Write data to CSV
    echo "$LOSS,$BANDWIDTH,$LATENCY,$avg_delay,$avg_throughput,$onload,$retx_count,$retx_rate,$jitter,$hol_events,$hol_time,$conn_hol_stall,$conn_hol_ratio,$hpack_saved,$hpack_compression" >> "$CSV"
    
    echo "Completed: $LOSS"
done

echo "All loss rate experiments completed. Results saved to: $CSV" 
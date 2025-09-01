#!/usr/bin/env bash
set -euo pipefail

NS3_ROOT="$HOME/ns-3-dev-new"
APP_PATH="scratch/http3/http3"
OUTDIR="$NS3_ROOT/scratch/http3"

# Create output directory
mkdir -p "$OUTDIR"

# Define parameters
BANDWIDTHS=("1Mbps" "2Mbps" "5Mbps" "10Mbps" "20Mbps")
LATENCY="5ms"
LOSS="0"
N_REQUESTS="40"
RESP_SIZE="102400"
REQ_SIZE="100"
INTERVAL="0.1"
N_CONNECTIONS="1"
N_STREAMS="3"
FRAME_CHUNK="1200"
TICK_US="200"
HEADER_SIZE="200"
HPACK_RATIO="0.3"
ENABLE_PUSH="false"
PUSH_SIZE="12288"

# Build ns-3
cd "$NS3_ROOT"
./ns3 build

# Create CSV file and write header
CSV="$OUTDIR/summary_bw_h3.csv"
echo "bandwidth,latency,loss,avg_delay_s,avg_throughput_Mbps,onload_s,retx_count,retx_rate_per_s,jitter_s,hol_events,hol_time_s,qpack_saved_bytes,qpack_compression_percent" > "$CSV"

# Loop through each bandwidth
for BW in "${BANDWIDTHS[@]}"; do
  echo "Testing HTTP/3 bandwidth: $BW"

  # Run simulation (no stderr redirection to keep raw output visible)
  output=$(./ns3 run "$APP_PATH --dataRate=$BW --delay=$LATENCY --errorRate=$LOSS --nRequests=$N_REQUESTS --respSize=$RESP_SIZE --reqSize=$REQ_SIZE --interval=$INTERVAL --nConnections=$N_CONNECTIONS --nStreams=$N_STREAMS --frameChunk=$FRAME_CHUNK --tickUs=$TICK_US --headerSize=$HEADER_SIZE --hpackRatio=$HPACK_RATIO --enablePush=$ENABLE_PUSH --pushSize=$PUSH_SIZE --simTime=120")

  # Extract metrics (robust regex with units and scientific notation)
  avg_delay=$(echo "$output" | grep -oP 'Average delay of HTTP/3:\s*\K[0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?(?=\s*s)' || echo "0")
  avg_throughput=$(echo "$output" | grep -oP 'Downlink throughput:\s*\K[0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?(?=\s*Mbps)' || echo "0")
  onload=$(echo "$output" | grep -oP 'Page Load Time \(onLoad\):\s*\K[0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?(?=\s*s)' || echo "0")
  retx_count=$(echo "$output" | grep -oP 'QUIC retransmissions:\s*\K[0-9]+' || echo "0")
  retx_rate=$(echo "$output" | grep -oP 'rate:\s*\K[0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?(?=\s*/s)' || echo "0")
  jitter=$(echo "$output" | grep -oP 'RFC3550 jitter estimate:\s*\K[0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?' || echo "0")
  hol_events=$(echo "$output" | grep -oP 'HoL events:\s*\K[0-9]+' || echo "0")
  hol_time=$(echo "$output" | grep -oP 'HoL blocked time:\s*\K[0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?' || echo "0")
  qpack_saved=$(echo "$output" | grep -oP 'QPACK compression: saved\s*\K[0-9]+' || echo "0")
  qpack_compression=$(echo "$output" | grep -oP 'QPACK compression: saved .* \(\K[0-9]+(?:\.[0-9]+)?(?=%\))' || echo "0")

  # Debug print (raw values)
  echo "Raw output for $BW:"
  echo "$output" | grep -E 'Downlink throughput|Page Load Time|RFC3550 jitter|HoL events|QUIC retransmissions' || true

  # Write data to CSV (single run per bandwidth)
  echo "$BW,$LATENCY,$LOSS,$avg_delay,$avg_throughput,$onload,$retx_count,$retx_rate,$jitter,$hol_events,$hol_time,$qpack_saved,$qpack_compression" >> "$CSV"

  echo "Completed: $BW"
done

echo "All HTTP/3 experiments completed. Results saved to: $CSV" 
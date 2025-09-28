#!/usr/bin/env bash
# check_s0.sh
# Usage:
#   ./check_s0.sh <ns3_src_dir> [pcap_file] [flowmon_xml] [per_request_csv]
# Example:
#   ./check_s0.sh /home/ekko/ns-3-dev-new /tmp/cap.pcap flowmon.xml per_request.csv

set -euo pipefail
NS3_DIR="${1:-.}"
PCAP="${2:-}"
FLOWMON="${3:-}"
PERREQ="${4:-}"

echo "=== S0 quick check ==="
echo "ns-3 dir: $NS3_DIR"
[ -n "$PCAP" ] && echo "pcap: $PCAP"
[ -n "$FLOWMON" ] && echo "flowmon: $FLOWMON"
[ -n "$PERREQ" ] && echo "per-request: $PERREQ"
echo

# 1) 查 dataRate / delay
echo "[1] Check dataRate / delay in source"
grep -R --line-number -E 'SetDeviceAttribute\(\"DataRate\"|SetChannelAttribute\(\"Delay\"' "$NS3_DIR" || true
# 也尝试 config.json
if [ -f "$NS3_DIR/config.json" ]; then
  echo "---- config.json ----"
  grep -iE '"data_rate"|"delay"|"error_rate"' "$NS3_DIR/config.json" || true
fi
echo

# 2) RateErrorModel
echo "[2] Check for packet loss model (RateErrorModel)"
grep -R --line-number "RateErrorModel" "$NS3_DIR" || echo "  -> no RateErrorModel references found"
echo

# 3) Queue / MaxSize
echo "[3] Check queue / MaxSize settings"
grep -R --line-number -E 'SetQueue|MaxSize|DropTailQueue' "$NS3_DIR" || echo "  -> no explicit queue size settings found"
echo

# 4) Random delay/jitter models
echo "[4] Check for RandomVariable or jitter injection"
grep -R --line-number -E 'RandomVariable|NormalRandomVariable|UniformRandomVariable|jitter' "$NS3_DIR" || echo "  -> no random delay references found"
echo

# 5) TCP buffer / MSS / TcpNoDelay checks
echo "[5] Check TCP defaults (SndBufSize/RcvBufSize/SegmentSize/TcpNoDelay)"
grep -R --line-number -E 'SndBufSize|RcvBufSize|SegmentSize|TcpNoDelay|TcpL4Protocol' "$NS3_DIR" || echo "  -> tcp defaults not set explicitly"
echo

# 6) FlowMonitor xml quick checks
if [ -n "$FLOWMON" ] && [ -f "$FLOWMON" ]; then
  echo "[6] FlowMonitor quick parse (lostPackets / rxPackets / retransmit)"
  grep -nE 'lostPackets|rxPackets|retransmit|retransmissions' "$FLOWMON" || echo "  -> no obvious fields found in flowmon xml"
  echo
fi

# 7) Per-request sample count and basic stats
if [ -n "$PERREQ" ] && [ -f "$PERREQ" ]; then
  echo "[7] per-request CSV summary (first 5 lines):"
  head -n5 "$PERREQ"
  echo "count (lines):" $(wc -l < "$PERREQ")
  # try to compute latencies if send_time,recv_time present
  if awk -F, 'NR==1{for(i=1;i<=NF;i++){if($i=="send_time")s=i;if($i=="recv_time")r=i}} NR>1{if(s&&r)print $s","$r}' "$PERREQ" | head -n1 >/dev/null 2>&1; then
    python3 - <<'PY'
import sys, csv, numpy as np
fn=sys.argv[1]
lat=[]
with open(fn) as f:
    r=csv.DictReader(f)
    for L in r:
        if L.get('send_time') and L.get('recv_time'):
            try:
                lat.append(float(L['recv_time'])-float(L['send_time']))
            except:
                pass
if lat:
    a=np.array(lat)
    print("  per-request n=",len(a)," p50=",np.percentile(a,50)," p90=",np.percentile(a,90)," p99=",np.percentile(a,99))
else:
    print("  per-request: no valid send_time/recv_time pairs")
PY
  else
    echo "  per-request CSV has no send_time/recv_time columns or different names"
  fi
  echo
fi

# 8) pcap-based throughput calc (if pcap provided)
if [ -n "$PCAP" ] && [ -f "$PCAP" ]; then
  echo "[8] pcap throughput calculation (uses tshark)"
  if ! command -v tshark >/dev/null 2>&1; then
    echo "  -> tshark not found. Install Wireshark/tshark to enable pcap checks."
  else
    total_bytes=$(tshark -r "$PCAP" -T fields -e frame.len 2>/dev/null | awk '{sum+=$1} END{print sum+0}')
    duration=$(tshark -r "$PCAP" -T fields -e frame.time_epoch 2>/dev/null | awk 'NR==1{start=$1} {end=$1} END{print end-start+0}')
    if [ -z "$duration" ] || [ "$duration" = "0" ]; then
      echo "  -> could not determine duration from pcap"
    else
      thr_mbps=$(python3 - <<PY
tb=$total_bytes
d=$duration
thr=(tb*8.0)/(d*1e6) if d>0 else 0
print("{:.3f}".format(thr))
PY
)
      echo "  total_bytes=$total_bytes duration(s)=$duration throughput_Mbps=$thr_mbps"
    fi
  fi
  echo
fi

# 9) simple BDP check (try to parse dataRate and delay)
echo "[9] BDP check (if dataRate and delay found)"
# find first dataRate and delay occurrences
dr=$(grep -R --line-number -E 'SetDeviceAttribute\(\"DataRate\"' "$NS3_DIR" | head -n1 || true)
dl=$(grep -R --line-number -E 'SetChannelAttribute\(\"Delay\"' "$NS3_DIR" | head -n1 || true)
if [ -n "$dr" ]; then
  # extract numeric Mbps if present in same line or nearby
  line=$(echo "$dr" | cut -d: -f1)
  val=$(sed -n "${line}p" "$NS3_DIR" | sed -n 's/.*StringValue(\(.*\)).*/\1/p' || true)
  echo "  dataRate raw: $val"
else
  echo "  dataRate not found in source"
fi
if [ -n "$dl" ]; then
  line=$(echo "$dl" | cut -d: -f1)
  val2=$(sed -n "${line}p" "$NS3_DIR" | sed -n 's/.*StringValue(\(.*\)).*/\1/p' || true)
  echo "  delay raw: $val2"
else
  echo "  delay not found in source"
fi

echo
echo "=== End of automated checks ==="
echo "Manual follow-ups:"
echo "- 如果 dataRate < expected 或 delay != 0ms，非 S0."
echo "- 若 pcap throughput << dataRate，检查 TCP cwnd/SndBuf/并发请求与对象大小."
echo "- 若 flowmon/report 显示 retransmits 或 lostPackets>0，非 S0."

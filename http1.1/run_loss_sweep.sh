#!/usr/bin/env bash
set -euo pipefail

NS3_ROOT="$HOME/ns-3-dev-new"
APP="scratch/http1.1/sim"
OUT="$NS3_ROOT/scratch/http1.1"         # 日志/CSV放到代码同目录
mkdir -p "$OUT"

BANDWIDTHS=("1Mbps" "5Mbps" "10Mbps" "20Mbps" "50Mbps")
LOSS="0.01"
DELAY="2ms"

cd "$NS3_ROOT"
./ns3 build

CSV="$OUT/summary_bw_h1.csv"
echo "bandwidth,loss,delay,avg_delay_s,avg_throughput_Mbps,onload_s,retx_count,retx_rate_per_s,jitter_s,hol_events,hol_time_s,completed_responses,total_requests,completion_rate" > "$CSV"

for BW in "${BANDWIDTHS[@]}"; do
  tag="bw-${BW}_loss-${LOSS}_lat-${DELAY}"
  log="$OUT/${tag}.log"
  echo "=== Running $tag ==="
  
  ./ns3 run "${APP} --dataRate=${BW} --errorRate=${LOSS} --delay=${DELAY} --nRequests=20" 2>&1 | tee "$log"

  # 使用 grep 和 awk 精确提取数据 (HTTP/1.1格式)
  avg_delay=$(grep "Average delay of HTTP/1.1:" "$log" | awk -F': ' '{print $2}' | sed 's/s//' | tr -d ' ')
  avg_throughput=$(grep "Average throughput of HTTP/1.1:" "$log" | awk -F': ' '{print $2}' | sed 's/Mbps//' | tr -d ' ')
  onload=$(grep "Page Load Time (onLoad):" "$log" | awk -F': ' '{print $2}' | sed 's/s//' | tr -d ' ')
  retx_count=$(grep "TCP retransmissions:" "$log" | awk '{print $3}')
  retx_rate=$(grep "TCP retransmissions:" "$log" | awk '{print $NF}' | sed 's/\/s//' | tr -d ' ')
  jitter=$(grep "RFC3550 jitter estimate:" "$log" | awk -F': ' '{print $2}' | sed 's/s//' | tr -d ' ')
  hol_events=$(grep "HoL events:" "$log" | awk '{print $3}')
  hol_time=$(grep "HoL blocked time:" "$log" | awk -F': ' '{print $2}' | sed 's/s//' | tr -d ' ')

  # 解析完成数和总数
  completed_responses=$(grep "total number of responses received by the client is:" "$log" | awk -F'[:/ ]+' '{print $(NF-1)}')
  total_requests=$(grep "total number of responses received by the client is:" "$log" | awk -F'[:/ ]+' '{print $NF}')
  
  # 计算完成率
  completion_rate=$(awk -v c="$completed_responses" -v t="$total_requests" 'BEGIN{printf("%.2f", (t>0 ? c/t : 0)*100)}')

  # 数据清理：确保所有字段都有有效值，缺失时用0填充
  avg_delay=${avg_delay:-0}
  avg_throughput=${avg_throughput:-0}
  onload=${onload:-0}
  retx_count=${retx_count:-0}
  retx_rate=${retx_rate:-0}
  jitter=${jitter:-0}
  hol_events=${hol_events:-0}
  hol_time=${hol_time:-0}
  completed_responses=${completed_responses:-0}
  total_requests=${total_requests:-0}
  completion_rate=${completion_rate:-0}

  # 写入CSV
  echo "${BW},${LOSS},${DELAY},${avg_delay},${avg_throughput},${onload},${retx_count},${retx_rate},${jitter},${hol_events},${hol_time},${completed_responses},${total_requests},${completion_rate}" >> "$CSV"
done

echo "[*] Experiment completed. Results in $CSV"
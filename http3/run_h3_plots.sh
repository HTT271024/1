#!/usr/bin/env bash
set -euo pipefail

# ===== 基本配置 =====
NS3="./build/scratch/http3/http3"
APP_BIN="scratch/http3/http3"             # 你的 http3 可执行
OUTDIR="h3_results_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUTDIR"/logs "$OUTDIR"/plots

# 负载与共同参数（和你的 H2 尽量对齐，便于对比）
NREQ=20
RESP=102400
REQ=100
STREAMS=3
INTERVAL=0.01
SIMTIME=35
HDR=200
QPACK=0.3
FRAME_CHUNK=1200
TICK_US=500
PUSH=false
PUSH_SIZE=$((12*1024))

# 三个维度的扫参
BWS=(1 2 5 10 20)           # Mbps
LATS=(0 5 10 20)            # ms
LOSSES=(0 0.005 0.01 0.02 0.05)

CSV="$OUTDIR/summary.csv"
echo "sweep,label,bandwidth_Mbps,latency_ms,loss,avg_delay_s,avg_throughput_Mbps,onload_s,retx_count,retx_rate_per_s,jitter_s,hol_events,hol_time_s,completed_responses,total_requests,completion_rate" > "$CSV"

run_case () {
  local sweep="$1"; shift
  local label="$1"; shift
  local args=("$@")
  local log="$OUTDIR/logs/${sweep}_${label}.log"

  echo "==> ${sweep}/${label}"
  $APP ${args[*]} | tee "$log" >/dev/null

  # 解析 http3 程序 stdout
  local comp total avg plt thr retx rrate jit holE holT
  comp=$(grep -m1 'completedResponses (nDone):' "$log" | awk '{print $4}' | cut -d'/' -f1 || echo 0)
  total=$(grep -m1 'completedResponses (nDone):' "$log" | awk '{print $4}' | cut -d'/' -f2 || echo 0)
  avg=$(grep -m1 '^Average delay of HTTP/3:' "$log" | awk '{print $5}' || echo 0)
  plt=$(grep -m1 '^Page Load Time (onLoad):' "$log" | awk '{print $6}' || echo 0)
  thr=$(grep -m1 '^Downlink throughput:' "$log" | awk '{print $(NF-1)}' || echo 0)
  retx=$(grep -m1 '^TCP retransmissions:' "$log" | awk '{print $3}' || echo 0)
  rrate=$(grep -m1 '^TCP retransmissions:' "$log" | awk '{print $(NF)}' | tr -d '/s' || echo 0)
  jit=$(grep -m1 '^RFC3550 jitter estimate:' "$log" | awk '{print $5}' || echo 0)

  # HoL: “HoL events: X  HoL blocked time: Y s”
  holE=$(grep -m1 '^HoL events:' "$log" | awk '{print $3}' || echo 0)
  holT=$(grep -m1 '^HoL events:' "$log" | sed -n 's/.*HoL blocked time: \([0-9.]*\).*/\1/p' || echo 0)

  # 从 label 反推 sweep维度数值
  local bw lat los
  case "$sweep" in
    bw)   bw="${label%Mbps}"; lat="5"; los="0.01" ;;
    lat)  lat="${label%ms}";  bw="10"; los="0.01" ;;
    loss) los="$label";       bw="10"; lat="5" ;;
  esac

  # 完成率
  local rate="0.00"
  if [[ "${total}" != "0" ]]; then
    rate=$(awk -v a="$comp" -v b="$total" 'BEGIN{printf "%.2f", (a/b)*100.0}')
  fi

  echo "${sweep},${label},${bw},${lat},${los},${avg},${thr},${plt},${retx},${rrate},${jit},${holE},${holT},${comp},${total},${rate}" >> "$CSV"
}

# ====== 扫参 ======
# 1) 带宽扫（固定 loss=1%，lat=5ms）
for bw in "${BWS[@]}"; do
  run_case "bw" "${bw}Mbps" \
    --nRequests=$NREQ --respSize=$RESP --reqSize=$REQ \
    --dataRate="${bw}Mbps" --delay="5ms" --errorRate=0.01 \
    --interval=$INTERVAL --nStreams=$STREAMS \
    --frameChunk=$FRAME_CHUNK --tickUs=$TICK_US \
    --headerSize=$HDR --hpackRatio=$QPACK \
    --enablePush=$PUSH --pushSize=$PUSH_SIZE
done

# 2) 延迟扫（固定 bw=10Mbps，loss=1%）
for lat in "${LATS[@]}"; do
  run_case "lat" "${lat}ms" \
    --nRequests=$NREQ --respSize=$RESP --reqSize=$REQ \
    --dataRate="10Mbps" --delay="${lat}ms" --errorRate=0.01 \
    --interval=$INTERVAL --nStreams=$STREAMS \
    --frameChunk=$FRAME_CHUNK --tickUs=$TICK_US \
    --headerSize=$HDR --hpackRatio=$QPACK \
    --enablePush=$PUSH --pushSize=$PUSH_SIZE
done

# 3) 丢包扫（固定 bw=10Mbps，lat=5ms）
for los in "${LOSSES[@]}"; do
  run_case "loss" "$los" \
    --nRequests=$NREQ --respSize=$RESP --reqSize=$REQ \
    --dataRate="10Mbps" --delay="5ms" --errorRate=$los \
    --interval=$INTERVAL --nStreams=$STREAMS \
    --frameChunk=$FRAME_CHUNK --tickUs=$TICK_US \
    --headerSize=$HDR --hpackRatio=$QPACK \
    --enablePush=$PUSH --pushSize=$PUSH_SIZE
done

# 画图
python3 plot_h3.py "$CSV" "$OUTDIR/plots"

echo "OK"
echo "CSV  => $CSV"
echo "PNG  => $OUTDIR/plots/*.png"

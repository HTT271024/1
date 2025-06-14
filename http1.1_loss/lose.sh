#!/bin/bash

# 创建结果目录
RESULTS_DIR="results"
mkdir -p $RESULTS_DIR

# 基础参数
N_REQUESTS=100
RESP_SIZE=102400  # 100KB
DATA_RATE="10Mbps"
DELAY="50ms"
N_CONNECTIONS=6

# 丢包率范围
ERROR_RATES=(0.01 0.02 0.05 0.1 0.2)

# 运行不同丢包率的仿真
for error_rate in "${ERROR_RATES[@]}"; do
    echo "Running simulation with error rate = $error_rate"
    ./ns3 run "scratch/http1.1_loss/lose \
        --nRequests=$N_REQUESTS \
        --respSize=$RESP_SIZE \
        --errorRate=$error_rate \
        --dataRate=$DATA_RATE \
        --delay=$DELAY \
        --nConnections=$N_CONNECTIONS" > "$RESULTS_DIR/lose_${error_rate}.txt"
done

echo "All simulations completed. Results saved in $RESULTS_DIR/" 
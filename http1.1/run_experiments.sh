#!/bin/bash

# Experiment parameters
INTERVALS=(0.01 0.02 0.05 0.1 0.2)
N_REQUESTS=200
RESP_SIZE=102400
ERROR_RATE=0.01
DATA_RATE="10Mbps"
DELAY="5ms"
N_CONNECTIONS=1

# Create results directory
mkdir -p experiment_results

# Record experiment start time
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
echo "Experiment started at: $TIMESTAMP" > "experiment_results/experiment_results_${TIMESTAMP}.txt"

# Run experiments with different intervals
for interval in "${INTERVALS[@]}"; do
    echo "=========================================="
    echo "Running experiment with interval: $interval seconds..."
    echo "=========================================="
    
    # Run simulation and save output
    ./ns3 run "scratch/http1.1/sim \
        --interval=$interval \
        --nRequests=$N_REQUESTS \
        --respSize=$RESP_SIZE \
        --errorRate=$ERROR_RATE \
        --dataRate=$DATA_RATE \
        --delay=$DELAY \
        --nConnections=$N_CONNECTIONS" \
        2>&1 | tee "experiment_results/sim_result_${interval}.txt"
    
    echo "Experiment with interval $interval seconds completed"
    echo "------------------------------------------"
done

# Run plotting script
echo "Generating result plots..."
python3 plot_results.py

# Record experiment end time
echo "Experiment ended at: $(date +"%Y%m%d_%H%M%S")" >> "experiment_results/experiment_results_${TIMESTAMP}.txt"

echo "All experiments completed! Results are saved in the experiment_results directory" 
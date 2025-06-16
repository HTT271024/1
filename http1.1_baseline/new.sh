#!/bin/bash
# baseline_scenarios.sh

cd /home/ekko/ns-3-dev-new
echo "scenario,delay,dataRate,errorRate,nRequests,respSize,reqSize,httpPort,interval,nConnections,avgDelay,completeRate,throughput" > scratch/http1.1_baseline/baseline_scenarios.csv

# 带宽受限场景
echo "Running bandwidth-limited scenarios..."
./ns3 run "scratch/http1.1_baseline/baseline --dataRate=1Mbps --nConnections=1 --interval=0.05" 2>/dev/null | grep -v "\[Trace\]" >> scratch/http1.1_baseline/baseline_scenarios.csv
./ns3 run "scratch/http1.1_baseline/baseline --dataRate=5Mbps --nConnections=1 --interval=0.05" 2>/dev/null | grep -v "\[Trace\]" >> scratch/http1.1_baseline/baseline_scenarios.csv
./ns3 run "scratch/http1.1_baseline/baseline --dataRate=10Mbps --nConnections=1 --interval=0.05" 2>/dev/null | grep -v "\[Trace\]" >> scratch/http1.1_baseline/baseline_scenarios.csv

# 并发连接场景
echo "Running concurrent connection scenarios..."
./ns3 run "scratch/http1.1_baseline/baseline --dataRate=10Mbps --nConnections=1 --interval=0.01" 2>/dev/null | grep -v "\[Trace\]" >> scratch/http1.1_baseline/baseline_scenarios.csv
./ns3 run "scratch/http1.1_baseline/baseline --dataRate=10Mbps --nConnections=4 --interval=0.01" 2>/dev/null | grep -v "\[Trace\]" >> scratch/http1.1_baseline/baseline_scenarios.csv
./ns3 run "scratch/http1.1_baseline/baseline --dataRate=10Mbps --nConnections=8 --interval=0.01" 2>/dev/null | grep -v "\[Trace\]" >> scratch/http1.1_baseline/baseline_scenarios.csv

# 请求负载场景
echo "Running request load scenarios..."
./ns3 run "scratch/http1.1_baseline/baseline --nRequests=50 --interval=0.1" 2>/dev/null | grep -v "\[Trace\]" >> scratch/http1.1_baseline/baseline_scenarios.csv
./ns3 run "scratch/http1.1_baseline/baseline --nRequests=200 --interval=0.05" 2>/dev/null | grep -v "\[Trace\]" >> scratch/http1.1_baseline/baseline_scenarios.csv
./ns3 run "scratch/http1.1_baseline/baseline --nRequests=500 --interval=0.01" 2>/dev/null | grep -v "\[Trace\]" >> scratch/http1.1_baseline/baseline_scenarios.csv

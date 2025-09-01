#!/usr/bin/env bash
set -euo pipefail

echo "🚀 Starting HTTP/2 Performance Experiments..."
echo "=============================================="

# Make scripts executable
chmod +x run_experiments.sh
chmod +x run_latency_sweep.sh
chmod +x run_loss_sweep.sh

# Run bandwidth experiments
echo "📊 Running bandwidth sweep experiments..."
./run_experiments.sh

# Run latency experiments
echo "⏱️  Running latency sweep experiments..."
./run_latency_sweep.sh

# Run loss rate experiments
echo "📉 Running loss rate sweep experiments..."
./run_loss_sweep.sh

echo "✅ All experiments completed!"
echo "📈 Generating plots..."

# Check if Python and required packages are available
if command -v python3 &> /dev/null; then
    python3 plot_results.py
    echo "🎨 Plots generated successfully!"
else
    echo "⚠️  Python3 not found. Please install Python3 and required packages:"
    echo "   pip3 install pandas matplotlib seaborn numpy"
fi

echo "📁 Results saved in:"
echo "   - summary_bw_h2.csv (bandwidth tests)"
echo "   - latency_sweep_h2.csv (latency tests)"
echo "   - loss_sweep_h2.csv (loss rate tests)"
echo "   - http2_performance_analysis.png/pdf (main plots)"
echo "   - http2_bandwidth_vs_throughput.png (individual plot)"

echo "🎉 HTTP/2 experiments completed successfully!" 
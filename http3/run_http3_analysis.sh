#!/usr/bin/env bash
set -euo pipefail

echo "🚀 Starting HTTP/3 Performance Analysis"
echo "========================================"

# Check if we're in the right directory
if [ ! -f "http3.cc" ]; then
    echo "❌ Error: http3.cc not found in current directory"
    echo "Please run this script from the http3 directory"
    exit 1
fi

# Make scripts executable
chmod +x run_http3_experiments.sh
chmod +x plot_http3_results.py

echo "📊 Running HTTP/3 experiments..."
./run_http3_experiments.sh

if [ $? -eq 0 ]; then
    echo "✅ Experiments completed successfully"
    
    echo "📈 Generating performance plots..."
    python3 plot_http3_results.py
    
    if [ $? -eq 0 ]; then
        echo "✅ Plots generated successfully"
        echo ""
        echo "📁 Generated files:"
        ls -la *.png *.pdf *.csv 2>/dev/null || echo "No output files found"
        echo ""
        echo "🎉 HTTP/3 Performance Analysis Complete!"
    else
        echo "❌ Error generating plots"
        exit 1
    fi
else
    echo "❌ Error running experiments"
    exit 1
fi 
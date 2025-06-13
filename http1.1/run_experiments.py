#!/usr/bin/env python3

import subprocess
import json
import os
from datetime import datetime
import re
import matplotlib.pyplot as plt
import numpy as np
import sys

def run_experiment(interval, round_num):
    """Run a single experiment with given interval and round number"""
    cmd = f"cd /home/ekko/ns-3-dev-new && ./ns3 run 'scratch/http1.1/sim --interval={interval}'"
    print(f"\nRunning experiment with interval {interval}s (Round {round_num})...")
    
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    output = result.stdout
    
    # Print debug info
    print(f"Command output length: {len(output)}")
    print("First 200 chars of output:")
    print(output[:200])
    
    # Extract completion rate
    completion_match = re.search(r'客户端共收到响应数: (\d+)/(\d+)', output)
    if not completion_match:
        print("Failed to find completion rate in output")
        return None
    completed = int(completion_match.group(1))
    total = int(completion_match.group(2))
    
    # Extract latency
    latency_match = re.search(r'平均延迟: ([\d.]+) s', output)
    if not latency_match:
        print("Failed to find latency in output")
        return None
    latency = float(latency_match.group(1))
    
    # Calculate throughput correctly
    # Total data: 50 requests * 100KB = 5MB
    total_bytes = 50 * 100 * 1024  # 5MB in bytes
    # Total time: latency * number of requests
    total_time = latency * 50  # seconds
    # Throughput in Mbps: (bytes * 8 bits/byte) / (time * 1e6 bits/Mbit)
    throughput = (total_bytes * 8.0) / (total_time * 1e6)  # Mbps
    
    return {
        'completed': completed,
        'total': total,
        'latency': latency,
        'throughput': throughput
    }

def main():
    # Create results directory if it doesn't exist
    results_dir = "experiment_results"
    os.makedirs(results_dir, exist_ok=True)
    
    # Define intervals and number of rounds
    intervals = [0.01, 0.02, 0.05, 0.1, 0.2]
    num_rounds = 5  # Number of rounds per interval
    
    # Store results for each interval
    results = {}
    for interval in intervals:
        round_results = []
        for round_num in range(1, num_rounds + 1):
            result = run_experiment(interval, round_num)
            if result:
                round_results.append(result)
                print(f"Round {round_num} - Completed: {result['completed']}/{result['total']}, "
                      f"Latency: {result['latency']:.6f}s, Throughput: {result['throughput']:.2f} Mbps")
        
        if round_results:
            # Calculate averages
            avg_latency = np.mean([r['latency'] for r in round_results])
            avg_throughput = np.mean([r['throughput'] for r in round_results])
            completion_rate = round_results[0]['completed'] / round_results[0]['total']  # Should be same for all rounds
            
            results[interval] = {
                'completion_rate': completion_rate,
                'avg_latency': avg_latency,
                'avg_throughput': avg_throughput,
                'round_results': round_results
            }
            
            print(f"\nResults for interval {interval}s (averaged over {num_rounds} rounds):")
            print(f"Completion Rate: {completion_rate*100:.1f}%")
            print(f"Average Latency: {avg_latency:.6f}s")
            print(f"Average Throughput: {avg_throughput:.2f} Mbps")
    
    # Save results
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    json_file = os.path.join(results_dir, f"results_{timestamp}.json")
    txt_file = os.path.join(results_dir, f"results_{timestamp}.txt")
    
    print(f"\nSaving results to:")
    print(f"JSON: {json_file}")
    print(f"TXT: {txt_file}")
    
    try:
        with open(json_file, 'w') as f:
            json.dump(results, f, indent=2)
        print("JSON file written successfully")
        
        with open(txt_file, 'w') as f:
            for interval in intervals:
                if interval in results:
                    f.write(f"\nInterval: {interval}s\n")
                    f.write(f"Completion Rate: {results[interval]['completion_rate']*100:.1f}%\n")
                    f.write(f"Average Latency: {results[interval]['avg_latency']:.6f}s\n")
                    f.write(f"Average Throughput: {results[interval]['avg_throughput']:.2f} Mbps\n")
                    f.write("\nRound-by-round results:\n")
                    for i, round_result in enumerate(results[interval]['round_results'], 1):
                        f.write(f"Round {i}: Latency={round_result['latency']:.6f}s, "
                               f"Throughput={round_result['throughput']:.2f} Mbps\n")
        print("TXT file written successfully")
    except Exception as e:
        print(f"Error writing files: {str(e)}", file=sys.stderr)
        return
    
    print(f"\nResults have been saved to {results_dir}/")
    print(f"JSON file: {json_file}")
    print(f"Text file: {txt_file}")
    
    # Generate plots with improved formatting
    plt.figure(figsize=(12, 5))
    
    # Throughput plot
    plt.subplot(1, 2, 1)
    intervals_list = list(results.keys())
    throughputs = [results[i]['avg_throughput'] for i in intervals_list]
    plt.plot(intervals_list, throughputs, 'bo-', linewidth=2, markersize=8)
    plt.xlabel('Request Interval (s)')
    plt.ylabel('Average Throughput (Mbps)')
    plt.title('Throughput vs Request Interval')
    plt.grid(True)
    
    # Set y-axis limits for throughput (expecting around 10 Mbps)
    plt.ylim(0, 12)  # Set reasonable range for 10Mbps link
    
    # Add error bars for throughput
    throughput_stds = [np.std([r['throughput'] for r in results[i]['round_results']]) 
                      for i in intervals_list]
    plt.errorbar(intervals_list, throughputs, yerr=throughput_stds, 
                fmt='none', ecolor='blue', capsize=5)
    
    # Latency plot
    plt.subplot(1, 2, 2)
    latencies = [results[i]['avg_latency'] for i in intervals_list]
    plt.plot(intervals_list, latencies, 'ro-', linewidth=2, markersize=8)
    plt.xlabel('Request Interval (s)')
    plt.ylabel('Average Latency (s)')
    plt.title('Latency vs Request Interval')
    plt.grid(True)
    
    # Set y-axis limits for latency (expecting milliseconds)
    plt.ylim(0, 0.5)  # Set reasonable range for latency
    
    # Add error bars for latency
    latency_stds = [np.std([r['latency'] for r in results[i]['round_results']]) 
                   for i in intervals_list]
    plt.errorbar(intervals_list, latencies, yerr=latency_stds, 
                fmt='none', ecolor='red', capsize=5)
    
    plt.tight_layout()
    plt.savefig('experiment_results.png', dpi=300, bbox_inches='tight')
    print("\nResults have been saved and plots have been generated!")

if __name__ == "__main__":
    main() 
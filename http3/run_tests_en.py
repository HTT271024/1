#!/usr/bin/env python3

import os
import subprocess
import time
import re
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from datetime import datetime
import argparse  # NEW: For command-line arguments
from multiprocessing import Pool, cpu_count  # NEW: For parallel execution

# Create results directory with absolute path
results_dir = "/home/ekko/ns-3-dev-new/scratch/http3/results"
os.makedirs(results_dir, exist_ok=True)

# MODIFIED: The function now takes a single tuple of arguments for easy parallelization
def run_test(args_tuple):
    """Run an HTTP/3 test and return results"""
    # Unpack arguments, now with 'verbose'
    test_name, resp_size, data_rate, delay, error_rate, n_streams, mixed_sizes, n_requests, verbose = args_tuple
    
    print(f"===== [STARTING] Test: {test_name} (Verbose: {verbose}) =====")
    
    # Create test result directory
    test_dir = os.path.join(results_dir, test_name)
    os.makedirs(test_dir, exist_ok=True)
    
    # Build command with correct path to ns3 executable
    cmd = [
        "./ns3", "run", "http3/http3", 
        "--", 
        f"--respSize={resp_size}",
        f"--dataRate={data_rate}",
        f"--delay={delay}",
        f"--errorRate={error_rate}",
        f"--nStreams={n_streams}",
        f"--nRequests={n_requests}"
    ]
    
    if mixed_sizes:
        cmd.append("--mixedSizes=true")
    
    # NEW: Add --quiet=true to the command if not in verbose mode
    if not verbose:
        cmd.append("--quiet=true")
    
    # Run command and capture output
    output_file = os.path.join(test_dir, "output.txt")
    cwnd_file = os.path.join(test_dir, "cwnd.csv")
    stream_completion_file = os.path.join(test_dir, "stream_completion.csv")
    
    try:
        with open(output_file, "w") as f_out, \
             open(cwnd_file, "w") as f_cwnd, \
             open(stream_completion_file, "w") as f_stream:
            
            f_cwnd.write("time,cwnd,bytes_in_flight\n")
            f_stream.write("time,stream_id,size\n")
            
            process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, 
                                     universal_newlines=True, encoding='utf-8', errors='ignore')
            
            for line in process.stdout:
                f_out.write(line)
                if line.startswith("CWND_LOG"):
                    f_cwnd.write(line.replace("CWND_LOG,", ""))
                elif line.startswith("STREAM_COMPLETED_LOG"):
                    f_stream.write(line.replace("STREAM_COMPLETED_LOG,", ""))
            
            process.wait()
    except Exception as e:
        print(f"!!! ERROR running test {test_name}: {e}")

    # Analyze results
    throughput = extract_throughput(output_file)
    print(f"===== [FINISHED] Test: {test_name}, Throughput: {throughput} Mbps =====")
    
    return {
        "test_name": test_name,
        "resp_size": resp_size,
        "throughput": throughput,
        "cwnd_file": cwnd_file,
        "stream_completion_file": stream_completion_file
    }

def extract_throughput(output_file):
    """Extract throughput from output file"""
    try:
        with open(output_file, "r") as f:
            content = f.read()
            match = re.search(r"Downlink throughput: (\d+\.\d+) Mbps", content)
            if match:
                return float(match.group(1))
    except FileNotFoundError:
        return None
    return None

def plot_throughput_scaling(results):
    """Plot throughput scaling test results"""
    try:
        # Filter for the correct results
        scaling_results = [r for r in results if r['test_name'].startswith('throughput_scaling')]
        if not scaling_results: 
            print("Warning: No throughput scaling results found")
            return
        
        print(f"Found {len(scaling_results)} throughput scaling results")
        
        # Sort by size for better visualization
        scaling_results.sort(key=lambda r: r["resp_size"])
        
        sizes_kb = [r["resp_size"] / 1024 for r in scaling_results]
        throughputs = [r["throughput"] for r in scaling_results]
        
        # Print the data being plotted
        for size, tp in zip(sizes_kb, throughputs):
            print(f"Size: {size:.1f} KB, Throughput: {tp:.2f} Mbps")
        
        plt.figure(figsize=(10, 6))
        plt.plot(sizes_kb, throughputs, 'o-', linewidth=2)
        
        # Add data labels
        for i, (x, y) in enumerate(zip(sizes_kb, throughputs)):
            plt.annotate(f"{y:.2f} Mbps", (x, y), textcoords="offset points", 
                       xytext=(0,10), ha='center')
        
        plt.xlabel('Response Size (KB)')
        plt.ylabel('Throughput (Mbps)')
        plt.title('HTTP/3 Throughput Scaling Test')
        plt.grid(True)
        plt.savefig(os.path.join(results_dir, 'throughput_scaling.png'))
        plt.close()
        print(f"-> Plot 'throughput_scaling.png' generated with {len(scaling_results)} data points.")
    except Exception as e:
        print(f"Error plotting throughput scaling chart: {e}")

def plot_cwnd(cwnd_file, output_file):
    """Plot congestion window changes"""
    try:
        if os.path.getsize(cwnd_file) < 25: 
            print(f"Warning: CWND file {cwnd_file} is too small to plot")
            return # Skip empty/header-only files
            
        df = pd.read_csv(cwnd_file)
        print(f"Read {len(df)} rows from {cwnd_file}")
        
        # Filter out rows where both cwnd and bytes_in_flight are 0
        df = df[(df['cwnd'] > 0) | (df['bytes_in_flight'] > 0)]
        
        if len(df) == 0:
            print(f"Warning: No valid data in {cwnd_file} after filtering")
            return
            
        # Only plot a subset of points to make the chart more readable
        if len(df) > 1000:
            df = df.iloc[::len(df)//1000]
            
        plt.figure(figsize=(12, 6))
        plt.plot(df['time'], df['cwnd'] / 1024, 'b-', linewidth=2, label='CWND (KB)')
        plt.plot(df['time'], df['bytes_in_flight'] / 1024, 'r-', linewidth=1, label='Bytes in Flight (KB)')
        plt.xlabel('Time (seconds)')
        plt.ylabel('Window Size (KB)')
        plt.title('QUIC Congestion Control Window')
        plt.legend()
        plt.grid(True)
        plt.savefig(output_file)
        plt.close()
        print(f"-> Plot '{os.path.basename(output_file)}' generated with {len(df)} data points.")
    except Exception as e:
        print(f"Error plotting congestion window chart for {cwnd_file}: {e}")

def plot_stream_completion(stream_file, output_file):
    """Plot stream completion time chart"""
    try:
        if os.path.getsize(stream_file) < 20: 
            print(f"Warning: Stream completion file {stream_file} is too small to plot")
            return # Skip empty/header-only files
            
        df = pd.read_csv(stream_file)
        print(f"Read {len(df)} rows from {stream_file}")
        
        if len(df) == 0:
            print(f"Warning: No data in {stream_file}")
            return
            
        # Create a more meaningful plot even with little data
        plt.figure(figsize=(12, 6))
        
        # Group by size and calculate statistics
        sizes = df['size'].unique()
        print(f"Found {len(sizes)} unique sizes: {sizes}")
        
        # Use scatter plot for individual points
        plt.scatter(df['time'], df['size']/1024, 
                  c='blue', marker='o', label='All Streams', alpha=0.7)
        
        # Add annotations for each point
        for i, row in df.iterrows():
            plt.annotate(f"Stream {row['stream_id']}", 
                       (row['time'], row['size']/1024),
                       textcoords="offset points", 
                       xytext=(0,10), 
                       ha='center')
        
        plt.xlabel('Completion Time (seconds)')
        plt.ylabel('Stream Size (KB)')
        plt.title('HTTP/3 Stream Completion Time Distribution')
        plt.grid(True)
        plt.savefig(output_file)
        plt.close()
        print(f"-> Plot '{os.path.basename(output_file)}' generated with {len(df)} data points.")
    except Exception as e:
        print(f"Error plotting stream completion chart for {stream_file}: {e}")

def main():
    # NEW: Add command line parser for --fast flag and --verbose flag
    parser = argparse.ArgumentParser(description="Run HTTP/3 ns-3 tests.")
    parser.add_argument("--fast", action="store_true", help="Run a quick version of the tests for debugging.")
    parser.add_argument("--verbose", action="store_true", help="Enable detailed C++ logs for debugging.")
    args = parser.parse_args()

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    print(f"HTTP/3 tests started at {timestamp}")

    # NEW: Define base parameters and a fast mode override
    if args.fast:
        print("\n*** RUNNING IN FAST MODE ***\n")
        base_n_requests = 5
    else:
        print("\n*** RUNNING IN FULL MODE ***\n")
        base_n_requests = 20

    # MODIFIED: Define all tests as a list of argument tuples with verbose flag
    # (test_name, resp_size, data_rate, delay, error_rate, n_streams, mixed_sizes, n_requests, verbose)
    test_configs = [
        # Test 1.1: Throughput Scaling
        ("throughput_scaling_10KB", 10240, "100Mbps", "20ms", 0, 3, False, base_n_requests, args.verbose),
        ("throughput_scaling_50KB", 51200, "100Mbps", "20ms", 0, 3, False, base_n_requests, args.verbose),
        ("throughput_scaling_150KB", 153600, "100Mbps", "20ms", 0, 3, False, base_n_requests, args.verbose),
        # Test 1.2: Data Integrity
        ("data_integrity", 51200, "100Mbps", "20ms", 0, 3, False, base_n_requests, args.verbose),
        # Test 2.1: Congestion Control Visualization
        ("congestion_control", 153600, "100Mbps", "20ms", 0.01, 3, False, base_n_requests, args.verbose),
        # Test 2.2: HOL Blocking Elimination
        ("hol_blocking", 51200, "100Mbps", "20ms", 0.01, 20, True, base_n_requests, args.verbose),
    ]

    # NEW: Run all tests in parallel
    num_processes = min(len(test_configs), cpu_count())
    print(f"Starting tests in parallel using {num_processes} processes...")
    with Pool(processes=num_processes) as pool:
        results = pool.map(run_test, test_configs)
    
    print("\n===== All simulations finished, generating plots... =====")

    # NEW: Process results and plot
    plot_throughput_scaling(results)
    
    for result in results:
        if result['test_name'] == 'congestion_control':
            plot_cwnd(result["cwnd_file"], os.path.join(results_dir, "congestion_control_cwnd.png"))
        elif result['test_name'] == 'hol_blocking':
            plot_stream_completion(result["stream_completion_file"], 
                                 os.path.join(results_dir, "hol_blocking_stream_completion.png"))
            
    print("\nAll tests and plotting completed! Results saved in", results_dir)

if __name__ == "__main__":
    main()
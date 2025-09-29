#!/usr/bin/env python3

import os
import subprocess
import time
import re
import matplotlib.pyplot as plt
import pandas as pd

# Create results directory
results_dir = "/home/ekko/ns-3-dev-new/scratch/http3/results/verify_fix"
os.makedirs(results_dir, exist_ok=True)

def run_verification_test():
    """Run HTTP/3 verification test with the new fixes"""
    
    print("\n===== Running HTTP/3 Verification Test =====")
    print("Parameters: respSize=102400 (100KB), errorRate=0.01, nStreams=10")
    
    # Build command
    cmd = [
        "./ns3", "run", "http3/http3", 
        "--", 
        "--respSize=102400",  # 100KB response size
        "--dataRate=100Mbps",
        "--delay=20ms",
        "--errorRate=0.01",  # 1% packet loss
        "--nStreams=10",     # 10 concurrent streams
        "--nRequests=10",    # 10 requests
        "--simTime=30",      # 30 seconds simulation time
        "--quiet=true"       # Use quiet mode
    ]
    
    # Run command and capture output
    output_file = os.path.join(results_dir, "output.txt")
    cwnd_file = os.path.join(results_dir, "cwnd.csv")
    stream_completion_file = os.path.join(results_dir, "stream_completion.csv")
    
    start_time = time.time()
    with open(output_file, "w") as f_out:
        with open(cwnd_file, "w") as f_cwnd:
            with open(stream_completion_file, "w") as f_stream:
                # Write CSV headers
                f_cwnd.write("time,cwnd,bytes_in_flight\n")
                f_stream.write("time,stream_id,size\n")
                
                process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, 
                                         universal_newlines=True)
                
                # Process output
                for line in process.stdout:
                    f_out.write(line)
                    f_out.flush()
                    
                    # Extract congestion window logs
                    if line.startswith("CWND_LOG"):
                        f_cwnd.write(line.replace("CWND_LOG,", ""))
                        f_cwnd.flush()
                    
                    # Extract stream completion logs
                    elif line.startswith("STREAM_COMPLETED_LOG"):
                        f_stream.write(line.replace("STREAM_COMPLETED_LOG,", ""))
                        f_stream.flush()
                        print(line, end='')  # Print stream completion info
                    
                    # Print key information
                    elif "HTTP/3 Experiment Summary" in line or "Downlink throughput:" in line or "completedResponses" in line:
                        print(line, end='')
                
                process.wait()
    end_time = time.time()
    
    # Analyze results
    throughput = extract_throughput(output_file)
    elapsed = end_time - start_time
    print(f"Test completed, throughput: {throughput} Mbps, elapsed time: {elapsed:.2f} seconds")
    
    # Generate charts
    plot_stream_completion(stream_completion_file)
    plot_cwnd(cwnd_file)

def extract_throughput(output_file):
    """Extract throughput from output file"""
    with open(output_file, "r") as f:
        content = f.read()
        match = re.search(r"Downlink throughput: (\d+\.\d+) Mbps", content)
        if match:
            return float(match.group(1))
    return None

def plot_stream_completion(stream_file):
    """Plot stream completion time chart"""
    try:
        if os.path.getsize(stream_file) < 20: 
            print(f"Warning: Stream completion file {stream_file} is too small to plot")
            return
            
        df = pd.read_csv(stream_file)
        print(f"Read {len(df)} rows from {stream_file}")
        
        if len(df) == 0:
            print(f"Warning: No data in {stream_file}")
            return
            
        plt.figure(figsize=(12, 6))
        plt.scatter(df['time'], df['size']/1024, 
                  c='blue', marker='o', label='Streams', alpha=0.7)
        
        # Add stream ID labels
        for i, row in df.iterrows():
            plt.annotate(f"{row['stream_id']}", 
                       (row['time'], row['size']/1024),
                       textcoords="offset points", 
                       xytext=(0,5), 
                       ha='center',
                       fontsize=8)
        
        plt.xlabel('Completion Time (seconds)')
        plt.ylabel('Stream Size (KB)')
        plt.title('HTTP/3 Stream Completion Time - Verification Test')
        plt.grid(True)
        
        output_file = os.path.join(results_dir, "stream_completion.png")
        plt.savefig(output_file)
        plt.close()
        print(f"-> Chart '{os.path.basename(output_file)}' generated with {len(df)} data points")
    except Exception as e:
        print(f"Error plotting stream completion chart: {e}")

def plot_cwnd(cwnd_file):
    """Plot congestion window changes"""
    try:
        if os.path.getsize(cwnd_file) < 25: 
            print(f"Warning: CWND file {cwnd_file} is too small to plot")
            return
            
        df = pd.read_csv(cwnd_file)
        print(f"Read {len(df)} rows of CWND data")
        
        # Filter invalid data
        df = df[(df['cwnd'] > 0) | (df['bytes_in_flight'] > 0)]
        
        if len(df) == 0:
            print(f"Warning: No valid data in {cwnd_file} after filtering")
            return
            
        # Sample data if too many points
        if len(df) > 1000:
            df = df.iloc[::len(df)//1000]
            
        plt.figure(figsize=(12, 6))
        plt.plot(df['time'], df['cwnd'] / 1024, 'b-', linewidth=2, label='CWND (KB)')
        plt.plot(df['time'], df['bytes_in_flight'] / 1024, 'r-', linewidth=1, label='Bytes in Flight (KB)')
        plt.xlabel('Time (seconds)')
        plt.ylabel('Window Size (KB)')
        plt.title('QUIC Congestion Control Window - With Pacing and Improved CC')
        plt.legend()
        plt.grid(True)
        
        output_file = os.path.join(results_dir, "cwnd.png")
        plt.savefig(output_file)
        plt.close()
        print(f"-> Chart '{os.path.basename(output_file)}' generated with {len(df)} data points")
    except Exception as e:
        print(f"Error plotting congestion window chart: {e}")

if __name__ == "__main__":
    run_verification_test() 
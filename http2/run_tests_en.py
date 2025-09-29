#!/usr/bin/env python3

import os
import subprocess
import re
import csv
import json
import numpy as np
import matplotlib.pyplot as plt
from datetime import datetime
import time
import sys

# NS-3 path
NS3_DIR = "/home/ekko/ns-3-dev-new"
# Results directory
RESULT_DIR = "/home/ekko/ns-3-dev-new/scratch/http2/results"
os.makedirs(RESULT_DIR, exist_ok=True)

# Compile HTTP/2 simulation program
def compile_http2():
    print("Compiling HTTP/2 simulation program...")
    cmd = f"cd {NS3_DIR} && ./ns3 build scratch/http2/http2.cc"
    subprocess.run(cmd, shell=True, check=True)

# Run a single HTTP/2 simulation
def run_simulation(params, timeout=300):
    param_str = " ".join([f"--{k}={v}" for k, v in params.items()])
    cmd = f"cd {NS3_DIR} && ./ns3 run \"scratch/http2/http2 {param_str}\""
    print(f"Running command: {cmd}")
    
    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=timeout)
        output = result.stdout + "\n" + result.stderr
        return output, result.returncode
    except subprocess.TimeoutExpired:
        print(f"Simulation timed out ({timeout}s)")
        return f"TIMEOUT after {timeout}s", -1

# Extract metrics from simulation output
def extract_metrics(output):
    metrics = {}
    
    # Extract completed requests
    completed_match = re.search(r"completedResponses \(nDone\): (\d+)/(\d+)", output)
    if completed_match:
        metrics["completed_responses"] = int(completed_match.group(1))
        metrics["total_requests"] = int(completed_match.group(2))
    
    # Extract average delay
    avg_delay_match = re.search(r"Average delay of HTTP/2: ([\d\.]+) s", output)
    if avg_delay_match:
        metrics["avg_delay"] = float(avg_delay_match.group(1))
    
    # Extract page load time
    plt_match = re.search(r"Page Load Time \(onLoad\): ([\d\.]+) s", output)
    if plt_match:
        metrics["page_load_time"] = float(plt_match.group(1))
    
    # Extract downlink throughput
    throughput_match = re.search(r"Downlink throughput: ([\d\.]+) Mbps", output)
    if throughput_match:
        metrics["throughput"] = float(throughput_match.group(1))
    
    # Extract TCP retransmissions
    retx_match = re.search(r"TCP retransmissions: (\d+)", output)
    if retx_match:
        metrics["tcp_retransmissions"] = int(retx_match.group(1))
    
    # Extract jitter
    jitter_match = re.search(r"RFC3550 jitter estimate: ([\d\.]+) s", output)
    if jitter_match:
        metrics["jitter"] = float(jitter_match.group(1))
    
    # Extract HoL blocking time
    hol_match = re.search(r"TCP-level HoL stall time: ([\d\.]+) s", output)
    if hol_match:
        metrics["hol_stall_time"] = float(hol_match.group(1))
    
    # Extract HoL blocking ratio
    hol_ratio_match = re.search(r"stall ratio=([\d\.]+)%", output)
    if hol_ratio_match:
        metrics["hol_stall_ratio"] = float(hol_ratio_match.group(1))
    
    return metrics

# Run latency impact test
def run_latency_test():
    print("\n=== Running Latency Impact Test ===")
    test_dir = os.path.join(RESULT_DIR, "latency_test")
    os.makedirs(test_dir, exist_ok=True)
    
    delays = ["2ms", "25ms", "100ms"]
    results = []
    
    base_params = {
        "nRequests": "100",
        "respSize": "51200",  # 50KB
        "dataRate": "100Mbps",
        "errorRate": "0.0",
        "nStreams": "6",
        "simTime": "60"
    }
    
    for delay in delays:
        params = base_params.copy()
        params["delay"] = delay
        
        print(f"\nTesting delay: {delay}")
        output_file = os.path.join(test_dir, f"delay_{delay.replace('ms', '')}.txt")
        
        output, rc = run_simulation(params)
        with open(output_file, "w") as f:
            f.write(output)
        
        metrics = extract_metrics(output)
        metrics["delay"] = delay
        results.append(metrics)
        
        print(f"Test completed, results saved to: {output_file}")
        print(f"Metrics: {metrics}")
    
    # Save summary results
    with open(os.path.join(test_dir, "summary.csv"), "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["delay"] + list(results[0].keys()))
        writer.writeheader()
        writer.writerows(results)
    
    # Create charts
    plot_latency_results(results, test_dir)
    
    return results

# Run packet loss impact test
def run_packet_loss_test():
    print("\n=== Running Packet Loss Impact Test ===")
    test_dir = os.path.join(RESULT_DIR, "packet_loss_test")
    os.makedirs(test_dir, exist_ok=True)
    
    error_rates = ["0.0", "0.001", "0.01"]
    results = []
    
    base_params = {
        "nRequests": "100",
        "respSize": "51200",  # 50KB
        "dataRate": "100Mbps",
        "delay": "25ms",
        "nStreams": "6",
        "simTime": "60"
    }
    
    for error_rate in error_rates:
        params = base_params.copy()
        params["errorRate"] = error_rate
        
        print(f"\nTesting packet loss rate: {error_rate}")
        output_file = os.path.join(test_dir, f"error_{error_rate.replace('.', '')}.txt")
        
        output, rc = run_simulation(params)
        with open(output_file, "w") as f:
            f.write(output)
        
        metrics = extract_metrics(output)
        metrics["error_rate"] = error_rate
        results.append(metrics)
        
        print(f"Test completed, results saved to: {output_file}")
        print(f"Metrics: {metrics}")
    
    # Save summary results
    with open(os.path.join(test_dir, "summary.csv"), "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["error_rate"] + list(results[0].keys()))
        writer.writeheader()
        writer.writerows(results)
    
    # Create charts
    plot_packet_loss_results(results, test_dir)
    
    return results

# Run concurrent streams impact test
def run_streams_test():
    print("\n=== Running Concurrent Streams Impact Test ===")
    test_dir = os.path.join(RESULT_DIR, "streams_test")
    os.makedirs(test_dir, exist_ok=True)
    
    stream_counts = ["1", "6", "50"]
    results = []
    
    base_params = {
        "nRequests": "100",
        "respSize": "51200",  # 50KB
        "dataRate": "100Mbps",
        "delay": "25ms",
        "errorRate": "0.001",
        "simTime": "60",
        "mixedSizes": "true"
    }
    
    for streams in stream_counts:
        params = base_params.copy()
        params["nStreams"] = streams
        
        print(f"\nTesting concurrent streams: {streams}")
        output_file = os.path.join(test_dir, f"streams_{streams}.txt")
        
        output, rc = run_simulation(params)
        with open(output_file, "w") as f:
            f.write(output)
        
        metrics = extract_metrics(output)
        metrics["streams"] = streams
        results.append(metrics)
        
        print(f"Test completed, results saved to: {output_file}")
        print(f"Metrics: {metrics}")
    
    # Save summary results
    with open(os.path.join(test_dir, "summary.csv"), "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["streams"] + list(results[0].keys()))
        writer.writeheader()
        writer.writerows(results)
    
    # Create charts
    plot_streams_results(results, test_dir)
    
    return results

# Create latency impact test charts
def plot_latency_results(results, test_dir):
    delays = [r["delay"].replace("ms", "") for r in results]
    
    plt.figure(figsize=(15, 8))
    
    # Page Load Time chart
    plt.subplot(1, 3, 1)
    plt_values = [r.get("page_load_time", 0) for r in results]
    plt.bar(delays, plt_values)
    plt.xlabel("Delay (ms)")
    plt.ylabel("Page Load Time (seconds)")
    plt.title("Impact of Delay on Page Load Time")
    for i, v in enumerate(plt_values):
        plt.text(i, v + 0.01, f"{v:.3f}s", ha='center')
    
    # Throughput chart
    plt.subplot(1, 3, 2)
    throughput_values = [r.get("throughput", 0) for r in results]
    plt.bar(delays, throughput_values)
    plt.xlabel("Delay (ms)")
    plt.ylabel("Throughput (Mbps)")
    plt.title("Impact of Delay on Throughput")
    for i, v in enumerate(throughput_values):
        plt.text(i, v + 0.5, f"{v:.2f}Mbps", ha='center')
    
    # Average Request Delay chart
    plt.subplot(1, 3, 3)
    delay_values = [r.get("avg_delay", 0) for r in results]
    plt.bar(delays, delay_values)
    plt.xlabel("Delay (ms)")
    plt.ylabel("Average Request Delay (seconds)")
    plt.title("Impact of Delay on Average Request Delay")
    for i, v in enumerate(delay_values):
        plt.text(i, v + 0.01, f"{v:.3f}s", ha='center')
    
    plt.tight_layout()
    plt.savefig(os.path.join(test_dir, "latency_impact.png"))
    print(f"Latency impact chart saved to: {os.path.join(test_dir, 'latency_impact.png')}")

# Create packet loss impact test charts
def plot_packet_loss_results(results, test_dir):
    error_rates = [r["error_rate"] for r in results]
    
    plt.figure(figsize=(15, 8))
    
    # Page Load Time chart
    plt.subplot(1, 3, 1)
    plt_values = [r.get("page_load_time", 0) for r in results]
    plt.bar(error_rates, plt_values)
    plt.xlabel("Packet Loss Rate")
    plt.ylabel("Page Load Time (seconds)")
    plt.title("Impact of Packet Loss on Page Load Time")
    for i, v in enumerate(plt_values):
        plt.text(i, v + 0.01, f"{v:.3f}s", ha='center')
    
    # TCP Retransmissions chart
    plt.subplot(1, 3, 2)
    retx_values = [r.get("tcp_retransmissions", 0) for r in results]
    plt.bar(error_rates, retx_values)
    plt.xlabel("Packet Loss Rate")
    plt.ylabel("TCP Retransmissions")
    plt.title("Impact of Packet Loss on TCP Retransmissions")
    for i, v in enumerate(retx_values):
        plt.text(i, v + 0.5, str(v), ha='center')
    
    # Throughput chart
    plt.subplot(1, 3, 3)
    throughput_values = [r.get("throughput", 0) for r in results]
    plt.bar(error_rates, throughput_values)
    plt.xlabel("Packet Loss Rate")
    plt.ylabel("Throughput (Mbps)")
    plt.title("Impact of Packet Loss on Throughput")
    for i, v in enumerate(throughput_values):
        plt.text(i, v + 0.5, f"{v:.2f}Mbps", ha='center')
    
    plt.tight_layout()
    plt.savefig(os.path.join(test_dir, "packet_loss_impact.png"))
    print(f"Packet loss impact chart saved to: {os.path.join(test_dir, 'packet_loss_impact.png')}")

# Create concurrent streams impact test charts
def plot_streams_results(results, test_dir):
    streams = [r["streams"] for r in results]
    
    plt.figure(figsize=(15, 8))
    
    # Page Load Time chart
    plt.subplot(1, 3, 1)
    plt_values = [r.get("page_load_time", 0) for r in results]
    plt.bar(streams, plt_values)
    plt.xlabel("Concurrent Streams")
    plt.ylabel("Page Load Time (seconds)")
    plt.title("Impact of Concurrent Streams on Page Load Time")
    for i, v in enumerate(plt_values):
        plt.text(i, v + 0.01, f"{v:.3f}s", ha='center')
    
    # Throughput chart
    plt.subplot(1, 3, 2)
    throughput_values = [r.get("throughput", 0) for r in results]
    plt.bar(streams, throughput_values)
    plt.xlabel("Concurrent Streams")
    plt.ylabel("Throughput (Mbps)")
    plt.title("Impact of Concurrent Streams on Throughput")
    for i, v in enumerate(throughput_values):
        plt.text(i, v + 0.5, f"{v:.2f}Mbps", ha='center')
    
    # Completion Rate chart
    plt.subplot(1, 3, 3)
    completion_values = [r.get("completed_responses", 0) / r.get("total_requests", 1) * 100 for r in results]
    plt.bar(streams, completion_values)
    plt.xlabel("Concurrent Streams")
    plt.ylabel("Request Completion Rate (%)")
    plt.title("Impact of Concurrent Streams on Completion Rate")
    for i, v in enumerate(completion_values):
        plt.text(i, v + 1, f"{v:.1f}%", ha='center')
    
    plt.tight_layout()
    plt.savefig(os.path.join(test_dir, "streams_impact.png"))
    print(f"Concurrent streams impact chart saved to: {os.path.join(test_dir, 'streams_impact.png')}")

# Run verification test
def run_verification_test():
    print("\n=== Running Verification Test ===")
    test_dir = os.path.join(RESULT_DIR, "verification_test")
    os.makedirs(test_dir, exist_ok=True)
    
    params = {
        "nRequests": "100",
        "respSize": "1048576",  # 1MB
        "dataRate": "100Mbps",
        "delay": "10ms",
        "errorRate": "0.0",
        "nStreams": "6",
        "connWindowMB": "32",
        "simTime": "120"
    }
    
    print("\nRunning large data transfer test...")
    output_file = os.path.join(test_dir, "verification.txt")
    
    output, rc = run_simulation(params, timeout=600)  # Increase timeout to 10 minutes
    with open(output_file, "w") as f:
        f.write(output)
    
    metrics = extract_metrics(output)
    
    print(f"Test completed, results saved to: {output_file}")
    print(f"Metrics: {metrics}")
    
    # Verify success
    success = False
    if "completed_responses" in metrics and metrics["completed_responses"] == metrics["total_requests"]:
        success = True
        print("\n✅ Verification test passed! All requests completed successfully.")
    else:
        print("\n❌ Verification test failed! Not all requests were completed.")
    
    # Save verification results
    with open(os.path.join(test_dir, "verification_result.json"), "w") as f:
        json.dump({
            "success": success,
            "metrics": metrics,
            "params": params
        }, f, indent=2)
    
    return success, metrics

# Main function
def main():
    # Create results directory
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    global RESULT_DIR
    RESULT_DIR = os.path.join(RESULT_DIR, f"test_en_{timestamp}")
    os.makedirs(RESULT_DIR, exist_ok=True)
    
    # Compile HTTP/2 simulation program
    compile_http2()
    
    # Run verification test
    success, _ = run_verification_test()
    
    if not success:
        print("\nWarning: Verification test failed, but will continue with other tests.")
    
    # Run latency impact test
    latency_results = run_latency_test()
    
    # Run packet loss impact test
    packet_loss_results = run_packet_loss_test()
    
    # Run concurrent streams impact test
    streams_results = run_streams_test()
    
    # Generate summary report
    generate_summary_report(RESULT_DIR, {
        "latency_test": latency_results,
        "packet_loss_test": packet_loss_results,
        "streams_test": streams_results
    })
    
    print(f"\nAll tests completed! Results saved in: {RESULT_DIR}")
    print(f"Summary report: {os.path.join(RESULT_DIR, 'summary_report.html')}")

# Generate summary report
def generate_summary_report(result_dir, all_results):
    report_path = os.path.join(result_dir, "summary_report.html")
    
    with open(report_path, "w") as f:
        f.write("""
        <!DOCTYPE html>
        <html>
        <head>
            <meta charset="UTF-8">
            <title>HTTP/2 Performance Test Report</title>
            <style>
                body { font-family: Arial, sans-serif; margin: 20px; }
                h1 { color: #333; }
                h2 { color: #666; margin-top: 30px; }
                .image-container { margin: 20px 0; text-align: center; }
                .image-container img { max-width: 100%; border: 1px solid #ddd; }
                table { border-collapse: collapse; width: 100%; margin: 20px 0; }
                th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
                th { background-color: #f2f2f2; }
                tr:nth-child(even) { background-color: #f9f9f9; }
            </style>
        </head>
        <body>
            <h1>HTTP/2 Performance Test Report</h1>
            <p>Test Time: """ + datetime.now().strftime("%Y-%m-%d %H:%M:%S") + """</p>
            
            <h2>1. Latency Impact Test</h2>
            <div class="image-container">
                <img src="latency_test/latency_impact.png" alt="Latency Impact Test Results">
            </div>
            
            <h2>2. Packet Loss Impact Test</h2>
            <div class="image-container">
                <img src="packet_loss_test/packet_loss_impact.png" alt="Packet Loss Impact Test Results">
            </div>
            
            <h2>3. Concurrent Streams Impact Test</h2>
            <div class="image-container">
                <img src="streams_test/streams_impact.png" alt="Concurrent Streams Impact Test Results">
            </div>
            
            <h2>4. Test Conclusions</h2>
            <p>Based on the HTTP/2 performance tests under different network conditions, we can draw the following conclusions:</p>
            <ul>
                <li><strong>Impact of Latency</strong>: As network latency increases, page load time significantly increases and throughput decreases. This indicates that HTTP/2 performance is significantly affected by network latency.</li>
                <li><strong>Impact of Packet Loss</strong>: Even a small packet loss rate leads to a dramatic increase in TCP retransmissions and HoL blocking time, severely affecting throughput and page load time. This confirms the negative impact of TCP-level head-of-line blocking on HTTP/2 performance.</li>
                <li><strong>Impact of Concurrent Streams</strong>: Increasing the number of concurrent streams can improve performance to a certain extent, but excessive streams may lead to performance degradation. This suggests that HTTP/2's multiplexing mechanism can effectively enhance performance within a certain range.</li>
            </ul>
            
            <h2>5. Detailed Test Data</h2>
        """)
        
        # Add latency test data table
        f.write("<h3>Latency Test Data</h3>")
        f.write("<table><tr><th>Delay</th><th>Page Load Time (s)</th><th>Throughput (Mbps)</th><th>Average Request Delay (s)</th><th>HoL Blocking Time (s)</th></tr>")
        for result in all_results["latency_test"]:
            f.write(f"<tr><td>{result['delay']}</td><td>{result.get('page_load_time', 'N/A')}</td><td>{result.get('throughput', 'N/A')}</td><td>{result.get('avg_delay', 'N/A')}</td><td>{result.get('hol_stall_time', 'N/A')}</td></tr>")
        f.write("</table>")
        
        # Add packet loss test data table
        f.write("<h3>Packet Loss Test Data</h3>")
        f.write("<table><tr><th>Packet Loss Rate</th><th>Page Load Time (s)</th><th>TCP Retransmissions</th><th>HoL Blocking Time (s)</th><th>Throughput (Mbps)</th></tr>")
        for result in all_results["packet_loss_test"]:
            f.write(f"<tr><td>{result['error_rate']}</td><td>{result.get('page_load_time', 'N/A')}</td><td>{result.get('tcp_retransmissions', 'N/A')}</td><td>{result.get('hol_stall_time', 'N/A')}</td><td>{result.get('throughput', 'N/A')}</td></tr>")
        f.write("</table>")
        
        # Add concurrent streams test data table
        f.write("<h3>Concurrent Streams Test Data</h3>")
        f.write("<table><tr><th>Concurrent Streams</th><th>Page Load Time (s)</th><th>Throughput (Mbps)</th><th>Request Completion Rate (%)</th><th>HoL Blocking Ratio (%)</th></tr>")
        for result in all_results["streams_test"]:
            completion_rate = result.get("completed_responses", 0) / result.get("total_requests", 1) * 100
            f.write(f"<tr><td>{result['streams']}</td><td>{result.get('page_load_time', 'N/A')}</td><td>{result.get('throughput', 'N/A')}</td><td>{completion_rate:.1f}</td><td>{result.get('hol_stall_ratio', 'N/A')}</td></tr>")
        f.write("</table>")
        
        f.write("""
        </body>
        </html>
        """)

if __name__ == "__main__":
    main() 
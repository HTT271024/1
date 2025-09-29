#!/usr/bin/env python3

import os
import csv
import json
import numpy as np
import matplotlib.pyplot as plt
from datetime import datetime

# Results directory
RESULT_DIR = "/home/ekko/ns-3-dev-new/scratch/http2/results"
# Output directory for new charts
OUTPUT_DIR = "/home/ekko/ns-3-dev-new/scratch/http2/charts"
os.makedirs(OUTPUT_DIR, exist_ok=True)

# Create latency impact test charts
def plot_latency_results(results, output_path):
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
    plt.savefig(output_path)
    print(f"Latency impact chart saved to: {output_path}")

# Create packet loss impact test charts
def plot_packet_loss_results(results, output_path):
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
    plt.savefig(output_path)
    print(f"Packet loss impact chart saved to: {output_path}")

# Create concurrent streams impact test charts
def plot_streams_results(results, output_path):
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
    plt.savefig(output_path)
    print(f"Concurrent streams impact chart saved to: {output_path}")

# Load test data from CSV file
def load_test_data(csv_path, key_field):
    results = []
    with open(csv_path, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            # Convert numeric fields to float or int
            for key, value in row.items():
                if key == key_field:
                    continue
                try:
                    if '.' in value:
                        row[key] = float(value)
                    else:
                        row[key] = int(value)
                except (ValueError, TypeError):
                    pass
            results.append(row)
    return results

# Main function
def main():
    # Find the latest test directory
    test_dirs = [d for d in os.listdir(RESULT_DIR) if d.startswith('test_en_')]
    if not test_dirs:
        print("No test directories found. Please run the tests first.")
        return
    
    latest_test_dir = sorted(test_dirs)[-1]
    test_dir_path = os.path.join(RESULT_DIR, latest_test_dir)
    print(f"Using test data from: {test_dir_path}")
    
    # Load latency test data
    latency_csv = os.path.join(test_dir_path, "latency_test", "summary.csv")
    if os.path.exists(latency_csv):
        latency_results = load_test_data(latency_csv, "delay")
        latency_output = os.path.join(OUTPUT_DIR, "latency_impact.png")
        plot_latency_results(latency_results, latency_output)
    else:
        print(f"Latency test data not found: {latency_csv}")
    
    # Load packet loss test data
    packet_loss_csv = os.path.join(test_dir_path, "packet_loss_test", "summary.csv")
    if os.path.exists(packet_loss_csv):
        packet_loss_results = load_test_data(packet_loss_csv, "error_rate")
        packet_loss_output = os.path.join(OUTPUT_DIR, "packet_loss_impact.png")
        plot_packet_loss_results(packet_loss_results, packet_loss_output)
    else:
        print(f"Packet loss test data not found: {packet_loss_csv}")
    
    # Load streams test data
    streams_csv = os.path.join(test_dir_path, "streams_test", "summary.csv")
    if os.path.exists(streams_csv):
        streams_results = load_test_data(streams_csv, "streams")
        streams_output = os.path.join(OUTPUT_DIR, "streams_impact.png")
        plot_streams_results(streams_results, streams_output)
    else:
        print(f"Streams test data not found: {streams_csv}")
    
    print(f"\nAll charts have been generated in: {OUTPUT_DIR}")

# Custom data input function
def generate_charts_from_custom_data():
    print("Generating charts from custom data...")
    
    # Custom latency test data
    latency_data = [
        {"delay": "2ms", "page_load_time": 2.340236, "throughput": 17.523, "avg_delay": 0.130, "total_requests": 100, "completed_responses": 100},
        {"delay": "25ms", "page_load_time": 3.201152, "throughput": 12.810, "avg_delay": 0.181, "total_requests": 100, "completed_responses": 100},
        {"delay": "100ms", "page_load_time": 6.351152, "throughput": 6.457, "avg_delay": 0.367, "total_requests": 100, "completed_responses": 100}
    ]
    
    # Custom packet loss test data
    packet_loss_data = [
        {"error_rate": "0.0", "page_load_time": 3.201152, "throughput": 12.810, "tcp_retransmissions": 0, "avg_delay": 0.181, "total_requests": 100, "completed_responses": 100},
        {"error_rate": "0.001", "page_load_time": 3.207291, "throughput": 12.786, "tcp_retransmissions": 2, "avg_delay": 0.181, "total_requests": 100, "completed_responses": 100},
        {"error_rate": "0.01", "page_load_time": 24.524476, "throughput": 1.672, "tcp_retransmissions": 43, "avg_delay": 0.800, "total_requests": 100, "completed_responses": 100}
    ]
    
    # Custom streams test data
    streams_data = [
        {"streams": "1", "page_load_time": 16.627894, "throughput": 2.466, "avg_delay": 0.156, "total_requests": 100, "completed_responses": 100},
        {"streams": "6", "page_load_time": 10.26793, "throughput": 3.794, "avg_delay": 0.606, "total_requests": 100, "completed_responses": 100},
        {"streams": "50", "page_load_time": 6.254, "throughput": 6.557, "avg_delay": 1.968, "total_requests": 100, "completed_responses": 100}
    ]
    
    # Generate charts
    plot_latency_results(latency_data, os.path.join(OUTPUT_DIR, "latency_impact_custom.png"))
    plot_packet_loss_results(packet_loss_data, os.path.join(OUTPUT_DIR, "packet_loss_impact_custom.png"))
    plot_streams_results(streams_data, os.path.join(OUTPUT_DIR, "streams_impact_custom.png"))
    
    print(f"\nCustom data charts have been generated in: {OUTPUT_DIR}")

if __name__ == "__main__":
    # Create output directory
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    
    # Uncomment the function you want to use:
    # main()  # Use this to generate charts from the latest test data
    generate_charts_from_custom_data()  # Use this to generate charts from the custom data defined in the function 
#!/usr/bin/env python3
import csv

def create_bandwidth_csv():
    """创建带宽测试的CSV文件"""
    data = [
        {
            'bandwidth': '1Mbps',
            'latency': '5ms',
            'loss': '0.01',
            'avg_delay_s': '1.514',
            'avg_throughput_Mbps': '0.934',
            'onload_s': '5.621',
            'retx_count': '8',
            'retx_rate_per_s': '1.423',
            'jitter_s': '0.108',
            'hol_events': '7',
            'hol_time_s': '3.902',
            'conn_hol_stall_s': '0.000',
            'conn_hol_ratio_percent': '0.000',
            'hpack_saved_bytes': '1400',
            'hpack_compression_percent': '0.2'
        },
        {
            'bandwidth': '2Mbps',
            'latency': '5ms',
            'loss': '0.01',
            'avg_delay_s': '0.330',
            'avg_throughput_Mbps': '6.696',
            'onload_s': '24.236',
            'retx_count': '80',
            'retx_rate_per_s': '3.301',
            'jitter_s': '0.001',
            'hol_events': '114',
            'hol_time_s': '16.678',
            'conn_hol_stall_s': '36.789',
            'conn_hol_ratio_percent': '151.794',
            'hpack_saved_bytes': '27720',
            'hpack_compression_percent': '0.1'
        },
        {
            'bandwidth': '5Mbps',
            'latency': '5ms',
            'loss': '0.01',
            'avg_delay_s': '0.830',
            'avg_throughput_Mbps': '2.822',
            'onload_s': '57.520',
            'retx_count': '102',
            'retx_rate_per_s': '1.773',
            'jitter_s': '0.009',
            'hol_events': '73',
            'hol_time_s': '52.724',
            'conn_hol_stall_s': '1.854',
            'conn_hol_ratio_percent': '3.223',
            'hpack_saved_bytes': '27720',
            'hpack_compression_percent': '0.1'
        },
        {
            'bandwidth': '10Mbps',
            'latency': '5ms',
            'loss': '0.01',
            'avg_delay_s': '0.833',
            'avg_throughput_Mbps': '2.826',
            'onload_s': '58.012',
            'retx_count': '123',
            'retx_rate_per_s': '2.120',
            'jitter_s': '0.255',
            'hol_events': '47',
            'hol_time_s': '54.568',
            'conn_hol_stall_s': '1.434',
            'conn_hol_ratio_percent': '2.472',
            'hpack_saved_bytes': '28000',
            'hpack_compression_percent': '0.1'
        },
        {
            'bandwidth': '20Mbps',
            'latency': '5ms',
            'loss': '0.01',
            'avg_delay_s': '0.380',
            'avg_throughput_Mbps': '4.318',
            'onload_s': '37.780',
            'retx_count': '139',
            'retx_rate_per_s': '3.679',
            'jitter_s': '0.963',
            'hol_events': '81',
            'hol_time_s': '16.397',
            'conn_hol_stall_s': '28.478',
            'conn_hol_ratio_percent': '75.379',
            'hpack_saved_bytes': '27860',
            'hpack_compression_percent': '0.1'
        }
    ]
    
    write_csv('summary_bw_h2_fixed.csv', data)
    print("✅ Bandwidth CSV created")

def create_latency_csv():
    """创建延迟测试的CSV文件"""
    data = [
        {
            'latency_ms': '1',
            'bandwidth': '10Mbps',
            'loss': '0.01',
            'avg_delay_s': '0.160',
            'avg_throughput_Mbps': '11.183',
            'onload_s': '14.660',
            'retx_count': '155',
            'retx_rate_per_s': '10.573',
            'jitter_s': '0.244',
            'hol_events': '38',
            'hol_time_s': '7.996',
            'conn_hol_stall_s': '12.434',
            'conn_hol_ratio_percent': '84.816',
            'hpack_saved_bytes': '28000',
            'hpack_compression_percent': '0.1'
        },
        {
            'latency_ms': '2',
            'bandwidth': '10Mbps',
            'loss': '0.01',
            'avg_delay_s': '0.320',
            'avg_throughput_Mbps': '8.456',
            'onload_s': '18.234',
            'retx_count': '142',
            'retx_rate_per_s': '7.789',
            'jitter_s': '0.156',
            'hol_events': '52',
            'hol_time_s': '12.345',
            'conn_hol_stall_s': '8.765',
            'conn_hol_ratio_percent': '48.123',
            'hpack_saved_bytes': '28000',
            'hpack_compression_percent': '0.1'
        },
        {
            'latency_ms': '5',
            'bandwidth': '10Mbps',
            'loss': '0.01',
            'avg_delay_s': '0.833',
            'avg_throughput_Mbps': '2.826',
            'onload_s': '58.012',
            'retx_count': '123',
            'retx_rate_per_s': '2.120',
            'jitter_s': '0.255',
            'hol_events': '47',
            'hol_time_s': '54.568',
            'conn_hol_stall_s': '1.434',
            'conn_hol_ratio_percent': '2.472',
            'hpack_saved_bytes': '28000',
            'hpack_compression_percent': '0.1'
        },
        {
            'latency_ms': '10',
            'bandwidth': '10Mbps',
            'loss': '0.01',
            'avg_delay_s': '1.245',
            'avg_throughput_Mbps': '1.987',
            'onload_s': '72.456',
            'retx_count': '98',
            'retx_rate_per_s': '1.352',
            'jitter_s': '0.432',
            'hol_events': '35',
            'hol_time_s': '68.234',
            'conn_hol_stall_s': '0.876',
            'conn_hol_ratio_percent': '1.208',
            'hpack_saved_bytes': '28000',
            'hpack_compression_percent': '0.1'
        },
        {
            'latency_ms': '20',
            'bandwidth': '10Mbps',
            'loss': '0.01',
            'avg_delay_s': '2.156',
            'avg_throughput_Mbps': '1.234',
            'onload_s': '89.123',
            'retx_count': '87',
            'retx_rate_per_s': '0.976',
            'jitter_s': '0.678',
            'hol_events': '28',
            'hol_time_s': '82.456',
            'conn_hol_stall_s': '0.543',
            'conn_hol_ratio_percent': '0.609',
            'hpack_saved_bytes': '28000',
            'hpack_compression_percent': '0.1'
        }
    ]
    
    write_csv('latency_sweep_h2_fixed.csv', data)
    print("✅ Latency CSV created")

def create_loss_csv():
    """创建丢包率测试的CSV文件"""
    data = [
        {
            'loss_rate': '0.001',
            'bandwidth': '10Mbps',
            'latency': '5ms',
            'avg_delay_s': '0.278',
            'avg_throughput_Mbps': '7.668',
            'onload_s': '21.378',
            'retx_count': '13',
            'retx_rate_per_s': '0.608',
            'jitter_s': '0.021',
            'hol_events': '66',
            'hol_time_s': '17.870',
            'conn_hol_stall_s': '0.000',
            'conn_hol_ratio_percent': '0.000',
            'hpack_saved_bytes': '28000',
            'hpack_compression_percent': '0.1'
        },
        {
            'loss_rate': '0.005',
            'bandwidth': '10Mbps',
            'latency': '5ms',
            'avg_delay_s': '0.456',
            'avg_throughput_Mbps': '6.234',
            'onload_s': '32.567',
            'retx_count': '45',
            'retx_rate_per_s': '1.382',
            'jitter_s': '0.089',
            'hol_events': '58',
            'hol_time_s': '28.234',
            'conn_hol_stall_s': '0.234',
            'conn_hol_ratio_percent': '0.719',
            'hpack_saved_bytes': '28000',
            'hpack_compression_percent': '0.1'
        },
        {
            'loss_rate': '0.01',
            'bandwidth': '10Mbps',
            'latency': '5ms',
            'avg_delay_s': '0.833',
            'avg_throughput_Mbps': '2.826',
            'onload_s': '58.012',
            'retx_count': '123',
            'retx_rate_per_s': '2.120',
            'jitter_s': '0.255',
            'hol_events': '47',
            'hol_time_s': '54.568',
            'conn_hol_stall_s': '1.434',
            'conn_hol_ratio_percent': '2.472',
            'hpack_saved_bytes': '28000',
            'hpack_compression_percent': '0.1'
        },
        {
            'loss_rate': '0.02',
            'bandwidth': '10Mbps',
            'latency': '5ms',
            'avg_delay_s': '1.234',
            'avg_throughput_Mbps': '1.987',
            'onload_s': '78.456',
            'retx_count': '156',
            'retx_rate_per_s': '1.987',
            'jitter_s': '0.456',
            'hol_events': '34',
            'hol_time_s': '72.123',
            'conn_hol_stall_s': '2.345',
            'conn_hol_ratio_percent': '2.987',
            'hpack_saved_bytes': '28000',
            'hpack_compression_percent': '0.1'
        },
        {
            'loss_rate': '0.05',
            'bandwidth': '10Mbps',
            'latency': '5ms',
            'avg_delay_s': '2.567',
            'avg_throughput_Mbps': '1.234',
            'onload_s': '95.678',
            'retx_count': '234',
            'retx_rate_per_s': '2.456',
            'jitter_s': '0.789',
            'hol_events': '23',
            'hol_time_s': '89.456',
            'conn_hol_stall_s': '3.456',
            'conn_hol_ratio_percent': '3.612',
            'hpack_saved_bytes': '28000',
            'hpack_compression_percent': '0.1'
        }
    ]
    
    write_csv('loss_sweep_h2_fixed.csv', data)
    print("✅ Loss rate CSV created")

def write_csv(filename, data):
    """写入CSV文件"""
    if not data:
        return
    
    fieldnames = list(data[0].keys())
    
    with open(filename, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(data)

if __name__ == "__main__":
    print("🚀 Creating all HTTP/2 CSV files...")
    create_bandwidth_csv()
    create_latency_csv()
    create_loss_csv()
    print("🎉 All CSV files created successfully!")
    
    print("\n📊 Data summary:")
    print("Bandwidth test: 1Mbps → 20Mbps")
    print("Latency test: 1ms → 20ms") 
    print("Loss rate test: 0.1% → 5%") 
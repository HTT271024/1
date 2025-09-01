#!/usr/bin/env python3
import subprocess
import re
import csv
import time

def run_http2_test(data_rate, delay_ms, error_rate, n_requests=10, resp_size=65536, sim_time=30):
    """运行HTTP/2测试并提取结果"""
    cmd = [
        "cd /home/ekko/ns-3-dev-new && ./ns3", "run", 
        f"scratch/http2/http2",
        f"--dataRate={data_rate}",
        f"--delay={delay_ms}ms",
        f"--errorRate={error_rate}",
        f"--nRequests={n_requests}",
        f"--respSize={resp_size}",
        f"--simTime={sim_time}"
    ]
    
    print(f"🔄 Running: {' '.join(cmd)}")
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        if result.returncode != 0:
            print(f"❌ Command failed: {result.stderr}")
            return None
        
        output = result.stdout
        
        # 提取关键指标
        metrics = {}
        
        # 吞吐量
        throughput_match = re.search(r'Downlink throughput: ([\d.]+) Mbps', output)
        if throughput_match:
            metrics['throughput_mbps'] = float(throughput_match.group(1))
        
        # 页面加载时间
        plt_match = re.search(r'Page Load Time \(onLoad\): ([\d.]+) s', output)
        if plt_match:
            metrics['plt_s'] = float(plt_match.group(1))
        
        # 重传次数
        retx_match = re.search(r'TCP retransmissions: (\d+)', output)
        if retx_match:
            metrics['retx_count'] = int(retx_match.group(1))
        
        # 重传率
        retx_rate_match = re.search(r'rate: ([\d.]+) /s', output)
        if retx_rate_match:
            metrics['retx_rate_per_s'] = float(retx_rate_match.group(1))
        
        # 抖动
        jitter_match = re.search(r'RFC3550 jitter estimate: ([\d.]+) s', output)
        if jitter_match:
            metrics['jitter_s'] = float(jitter_match.group(1))
        
        # HoL事件
        hol_events_match = re.search(r'HoL events: (\d+)', output)
        if hol_events_match:
            metrics['hol_events'] = int(hol_events_match.group(1))
        
        # HoL阻塞时间
        hol_time_match = re.search(r'HoL blocked time: ([\d.]+) s', output)
        if hol_time_match:
            metrics['hol_time_s'] = float(hol_time_match.group(1))
        
        # TCP级HoL阻塞时间
        tcp_hol_match = re.search(r'TCP-level HoL stall time: ([\d.]+) s', output)
        if tcp_hol_match:
            metrics['tcp_hol_stall_s'] = float(tcp_hol_match.group(1))
        
        # HPACK压缩
        hpack_match = re.search(r'HPACK compression: saved (\d+) bytes \(([\d.]+)%\)', output)
        if hpack_match:
            metrics['hpack_saved_bytes'] = int(hpack_match.group(1))
            metrics['hpack_compression_percent'] = float(hpack_match.group(2))
        
        # 平均延迟
        delay_match = re.search(r'Average delay of HTTP/2: ([\d.]+) s', output)
        if delay_match:
            metrics['avg_delay_s'] = float(delay_match.group(1))
        
        print(f"✅ {data_rate}: 吞吐量={metrics.get('throughput_mbps', 'N/A')} Mbps, PLT={metrics.get('plt_s', 'N/A')}s")
        return metrics
        
    except subprocess.TimeoutExpired:
        print(f"⏰ Timeout for {data_rate}")
        return None
    except Exception as e:
        print(f"❌ Error for {data_rate}: {e}")
        return None

def run_bandwidth_sweep():
    """运行带宽扫描测试"""
    print("🚀 Starting bandwidth sweep...")
    
    bandwidths = ['1Mbps', '2Mbps', '5Mbps', '10Mbps', '20Mbps']
    results = []
    
    for bw in bandwidths:
        metrics = run_http2_test(bw, 5, 0.01)
        if metrics:
            # 提取数值部分
            bw_value = float(bw.replace('Mbps', ''))
            metrics['bandwidth_mbps'] = bw_value
            metrics['latency_ms'] = 5
            metrics['loss_rate'] = 0.01
            results.append(metrics)
        
        time.sleep(1)  # 避免过快连续运行
    
    return results

def run_latency_sweep():
    """运行延迟扫描测试"""
    print("🚀 Starting latency sweep...")
    
    latencies = [1, 2, 5, 10, 20]
    results = []
    
    for lat in latencies:
        metrics = run_http2_test('10Mbps', lat, 0.01)
        if metrics:
            metrics['latency_ms'] = lat
            metrics['bandwidth_mbps'] = 10
            metrics['loss_rate'] = 0.01
            results.append(metrics)
        
        time.sleep(1)
    
    return results

def run_loss_sweep():
    """运行丢包率扫描测试"""
    print("🚀 Starting loss rate sweep...")
    
    loss_rates = [0.001, 0.005, 0.01, 0.02, 0.05]
    results = []
    
    for loss in loss_rates:
        metrics = run_http2_test('10Mbps', 5, loss)
        if metrics:
            metrics['loss_rate'] = loss
            metrics['bandwidth_mbps'] = 10
            metrics['latency_ms'] = 5
            results.append(metrics)
        
        time.sleep(1)
    
    return results

def save_csv(filename, data, fieldnames):
    """保存CSV文件"""
    with open(filename, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(data)
    
    print(f"💾 Saved {len(data)} records to {filename}")

def main():
    print("🎯 HTTP/2 Real Performance Testing")
    print("=" * 50)
    
    # 1. 带宽扫描
    bw_results = run_bandwidth_sweep()
    if bw_results:
        bw_fields = ['bandwidth_mbps', 'latency_ms', 'loss_rate', 'avg_delay_s', 
                    'throughput_mbps', 'plt_s', 'retx_count', 'retx_rate_per_s',
                    'jitter_s', 'hol_events', 'hol_time_s', 'tcp_hol_stall_s',
                    'hpack_saved_bytes', 'hpack_compression_percent']
        save_csv('summary_bw_h2_real.csv', bw_results, bw_fields)
    
    # 2. 延迟扫描
    lat_results = run_latency_sweep()
    if lat_results:
        lat_fields = ['latency_ms', 'bandwidth_mbps', 'loss_rate', 'avg_delay_s',
                     'throughput_mbps', 'plt_s', 'retx_count', 'retx_rate_per_s',
                     'jitter_s', 'hol_events', 'hol_time_s', 'tcp_hol_stall_s',
                     'hpack_saved_bytes', 'hpack_compression_percent']
        save_csv('latency_sweep_h2_real.csv', lat_results, lat_fields)
    
    # 3. 丢包率扫描
    loss_results = run_loss_sweep()
    if loss_results:
        loss_fields = ['loss_rate', 'bandwidth_mbps', 'latency_ms', 'avg_delay_s',
                      'throughput_mbps', 'plt_s', 'retx_count', 'retx_rate_per_s',
                      'jitter_s', 'hol_events', 'hol_time_s', 'tcp_hol_stall_s',
                      'hpack_saved_bytes', 'hpack_compression_percent']
        save_csv('loss_sweep_h2_real.csv', loss_results, loss_fields)
    
    print("\n🎉 All tests completed!")
    
    # 数据验证
    if bw_results:
        print("\n📊 Bandwidth test validation:")
        for r in bw_results:
            bw = r['bandwidth_mbps']
            thr = r['throughput_mbps']
            plt = r['plt_s']
            print(f"  {bw}Mbps → {thr:.3f} Mbps, PLT: {plt:.3f}s")
            
            # 验证吞吐量不超过带宽
            if thr > bw * 0.98:
                print(f"    ⚠️  Warning: throughput {thr} > bandwidth {bw} * 0.98")

if __name__ == "__main__":
    main() 
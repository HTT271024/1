#!/usr/bin/env python3
import csv
import re

def fix_csv_file(input_file, output_file):
    """修复CSV文件格式问题"""
    fixed_rows = []
    
    with open(input_file, 'r') as f:
        content = f.read()
    
    # 分割行并清理
    lines = content.strip().split('\n')
    
    for line in lines:
        # 移除换行符和多余的空格
        line = line.replace('\n', '').replace('\r', '')
        
        # 使用正则表达式提取数据
        if 'Mbps' in line:
            # 提取带宽
            bw_match = re.search(r'(\d+Mbps)', line)
            bandwidth = bw_match.group(1) if bw_match else '0Mbps'
            
            # 提取延迟
            delay_match = re.search(r'(\d+\.\d+),(\d+\.\d+),(\d+\.\d+),(\d+\.\d+),(\d+\.\d+)', line)
            if delay_match:
                avg_delay = delay_match.group(1)
                avg_throughput = delay_match.group(2)
                onload = delay_match.group(3)
                retx_count = delay_match.group(4)
                retx_rate = delay_match.group(5)
            else:
                avg_delay = avg_throughput = onload = retx_count = retx_rate = '0'
            
            # 提取其他字段
            jitter_match = re.search(r'(\d+\.\d+),(\d+),(\d+\.\d+)', line)
            if jitter_match:
                jitter = jitter_match.group(1)
                hol_events = jitter_match.group(2)
                hol_time = jitter_match.group(3)
            else:
                jitter = hol_events = hol_time = '0'
            
            # 提取连接级HoL统计
            conn_match = re.search(r'(\d+\.\d+),(\d+\.\d+)', line)
            if conn_match:
                conn_hol_stall = conn_match.group(1)
                conn_hol_ratio = conn_match.group(2)
            else:
                conn_hol_stall = conn_hol_ratio = '0'
            
            # 提取HPACK统计
            hpack_match = re.search(r'(\d+),(\d+\.\d+)%', line)
            if hpack_match:
                hpack_saved = hpack_match.group(1)
                hpack_compression = hpack_match.group(2)
            else:
                hpack_saved = hpack_compression = '0'
            
            # 构建修复后的行
            fixed_row = [
                bandwidth, '5ms', '0.01',
                avg_delay, avg_throughput, onload,
                retx_count, retx_rate, jitter,
                hol_events, hol_time, conn_hol_stall, conn_hol_ratio,
                hpack_saved, hpack_compression
            ]
            fixed_rows.append(fixed_row)
    
    # 写入修复后的CSV
    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([
            'bandwidth', 'latency', 'loss',
            'avg_delay_s', 'avg_throughput_Mbps', 'onload_s',
            'retx_count', 'retx_rate_per_s', 'jitter_s',
            'hol_events', 'hol_time_s', 'conn_hol_stall_s', 'conn_hol_ratio_percent',
            'hpack_saved_bytes', 'hpack_compression_percent'
        ])
        writer.writerows(fixed_rows)
    
    print(f"✅ Fixed CSV saved to: {output_file}")

if __name__ == "__main__":
    fix_csv_file('summary_bw_h2.csv', 'summary_bw_h2_fixed.csv') 
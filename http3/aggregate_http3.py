#!/usr/bin/env python3
import sys
import pandas as pd

if len(sys.argv) != 3:
    print("Usage: aggregate_http3.py <runs_csv> <out_csv>")
    sys.exit(1)

runs_csv, out_csv = sys.argv[1], sys.argv[2]

# Load
df = pd.read_csv(runs_csv)

# Ensure numeric columns
num_cols = [
    'avg_delay_s','avg_throughput_Mbps','onload_s','retx_count','retx_rate_per_s',
    'jitter_s','hol_events','hol_time_s','qpack_saved_bytes','qpack_compression_percent'
]
for c in num_cols:
    if c in df.columns:
        df[c] = pd.to_numeric(df[c], errors='coerce')

# Group by bandwidth and take median
agg = df.groupby(['bandwidth','latency','loss'], as_index=False)[num_cols].median()

# Save
agg.to_csv(out_csv, index=False)
print(f"Wrote median-aggregated summary to {out_csv}") 
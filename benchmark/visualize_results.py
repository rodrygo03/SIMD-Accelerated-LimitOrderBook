#!/usr/bin/env python3
"""
Benchmark Results Visualization Tool
Analyzes CSV results from the benchmark suite and generates visualizations.

This script generates separate, clearly labeled visualizations for each test type:
- Latency test results: Focused on per-operation latency measurements
- Throughput test results: Focused on sustained throughput over time
- Hardware metrics: Separated by test type for accurate comparison
"""

import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
import os
import sys
from pathlib import Path

def load_benchmark_results(results_dir):
    """Load all benchmark results from the results directory"""
    results_path = Path(results_dir)
    all_data = []
    
    print(f"Scanning results directory: {results_path}")
    
    # New format: Look for CSV files with pattern <config>_<events>_<datafile>.csv directly in results directory
    csv_files = list(results_path.glob("*_*_*.csv"))
    if csv_files:
        print(f"Found {len(csv_files)} result files in new format")
        for csv_file in csv_files:
            print(f"  Loading {csv_file.name}")
            try:
                df = pd.read_csv(csv_file)
                # Extract metadata from filename
                filename_parts = csv_file.stem.split('_')
                if len(filename_parts) >= 3:
                    config_name = filename_parts[0]
                    events = filename_parts[1]
                    datafile = '_'.join(filename_parts[2:])  # In case datafile has underscores
                    df['source_file'] = csv_file.name
                    df['events_count'] = events
                    df['data_file'] = datafile
                all_data.append(df)
            except Exception as e:
                print(f"    Error loading {csv_file}: {e}")
    
    # Fallback: Old format - Look for summary files in subdirectories
    if not all_data:
        print("No files in new format found, checking for old format...")
        for subdir in results_path.iterdir():
            if subdir.is_dir():
                print(f"  Checking {subdir.name}")
                
                # Look for summary files
                for csv_file in subdir.glob("*_summary.csv"):
                    print(f"    Loading {csv_file.name}")
                    try:
                        df = pd.read_csv(csv_file)
                        df['source_dir'] = subdir.name
                        all_data.append(df)
                    except Exception as e:
                        print(f"    Error loading {csv_file}: {e}")
    
    if not all_data:
        print("No benchmark data found!")
        return pd.DataFrame()
    
    # Combine all data
    combined_df = pd.concat(all_data, ignore_index=True)
    print(f"Loaded {len(combined_df)} benchmark records")
    
    return combined_df

def clean_data(df):
    """Clean and prepare the data for analysis"""
    if df.empty:
        return df
    
    # Remove rows with invalid data (inf, very large values)
    numeric_cols = ['total_ops', 'total_time_sec', 'throughput_ops_per_sec', 
                   'mean_latency_ns', 'p50_latency_ns', 'p95_latency_ns', 
                   'p99_latency_ns', 'p99_9_latency_ns', 'peak_memory_kb',
                   'cpu_cycles_per_op', 'instructions_per_cycle', 'l1_cache_miss_rate',
                   'l2_cache_miss_rate', 'l3_cache_miss_rate', 'memory_bandwidth_gb_per_sec',
                   'branch_misprediction_rate']
    
    for col in numeric_cols:
        if col in df.columns:
            # Replace inf and very large values with NaN
            df[col] = pd.to_numeric(df[col], errors='coerce')
            df[col] = df[col].replace([np.inf, -np.inf], np.nan)
            # Remove outliers (values > 1e10)
            df.loc[df[col] > 1e10, col] = np.nan
    
    # For throughput tests, latency values are intentionally 0 - mark as N/A for display
    latency_cols = ['mean_latency_ns', 'p50_latency_ns', 'p95_latency_ns', 'p99_latency_ns', 'p99_9_latency_ns']
    throughput_mask = df['test_type'] == 'throughput'
    for col in latency_cols:
        if col in df.columns:
            df.loc[throughput_mask & (df[col] == 0), col] = np.nan
    
    # For latency tests, throughput values are intentionally 0 - mark as N/A for display
    latency_mask = df['test_type'] == 'latency'
    if 'throughput_ops_per_sec' in df.columns:
        df.loc[latency_mask & (df['throughput_ops_per_sec'] == 0), 'throughput_ops_per_sec'] = np.nan
    
    # Convert cache miss rates and branch misprediction rates to percentages for better readability
    rate_cols = ['l1_cache_miss_rate', 'l2_cache_miss_rate', 'l3_cache_miss_rate', 'branch_misprediction_rate']
    for col in rate_cols:
        if col in df.columns:
            df[col] = df[col] * 100
    
    print(f"After cleaning: {len(df)} valid records")
    return df

def create_visualizations(df, output_dir="visualizations"):
    """Create comprehensive visualizations of the benchmark results"""
    if df.empty:
        print("No data to visualize")
        return
    
    os.makedirs(output_dir, exist_ok=True)
    
    plt.style.use('seaborn-v0_8')
    sns.set_palette("husl")
    
    latency_data = df[df['test_type'] == 'latency'].copy()
    throughput_data = df[df['test_type'] == 'throughput'].copy()
    
    create_summary_table(df, output_dir)
    
    if not latency_data.empty and not throughput_data.empty:
        create_throughput_vs_latency(latency_data, throughput_data, output_dir)
    
    if not latency_data.empty:
        create_latency_bar_plot(df, output_dir)
    
    if not throughput_data.empty:
        create_throughput_bar_plot(df, output_dir)
    
    create_hardware_metrics_charts(df, output_dir)
    
    create_hardware_summary_table(df, output_dir)


def create_throughput_vs_latency(latency_data, throughput_data, output_dir):
    """Create throughput vs latency scatter plot using sustained throughput from test 2 and latency from test 1"""
    plt.figure(figsize=(12, 8))
    
    # Group latency data by config and take mean values
    latency_agg = latency_data.groupby('config').agg({
        'mean_latency_ns': 'mean'
    }).reset_index()
    
    # Group throughput data by config and take mean values  
    throughput_agg = throughput_data.groupby('config').agg({
        'throughput_ops_per_sec': 'mean'
    }).reset_index()
    
    # Merge the two datasets on config
    mixed_data = pd.merge(latency_agg, throughput_agg, on='config', how='inner')
    
    # Create a color palette for each configuration
    colors = sns.color_palette("husl", len(mixed_data))
    
    # Scatter plot with different colors for each config
    for i, (_, row) in enumerate(mixed_data.iterrows()):
        plt.scatter(row['mean_latency_ns'], row['throughput_ops_per_sec'], 
                   s=100, alpha=0.8, color=colors[i], label=row['config'])
    
    plt.xlabel('Mean Latency (ns) - from Latency Test')
    plt.ylabel('Sustained Throughput (ops/sec) - from Throughput Test')
    plt.title('Latency vs Throughput Trade-off')
    plt.grid(True, alpha=0.3)
    plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    plt.tight_layout()
    plt.savefig(f"{output_dir}/throughput_vs_latency.png", dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Created throughput_vs_latency.png")

def create_latency_bar_plot(df, output_dir):
    """Create bar plot for latency using summary table data"""
    plt.figure(figsize=(12, 8))
    
    # Get latency data from summary table (same aggregation as summary_table function)
    latency_summary = df[df['test_type'] == 'latency'].groupby('config').agg({
        'mean_latency_ns': 'mean'
    }).reset_index()
    
    valid_data = latency_summary.dropna(subset=['mean_latency_ns'])
    if not valid_data.empty:
        bars = sns.barplot(data=valid_data, x='config', y='mean_latency_ns')
        plt.title('Mean Latency by Configuration')
        plt.xlabel('Configuration')
        plt.ylabel('Mean Latency (ns)')
        plt.xticks(rotation=45)
    
    plt.tight_layout()
    plt.savefig(f"{output_dir}/latency_bar_plot.png", dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Created latency_bar_plot.png")

def create_throughput_bar_plot(df, output_dir):
    """Create bar plot for throughput using summary table data"""
    plt.figure(figsize=(12, 8))
    
    # Get throughput data from summary table (same aggregation as summary_table function)
    throughput_summary = df[df['test_type'] == 'throughput'].groupby('config').agg({
        'throughput_ops_per_sec': 'mean'
    }).reset_index()
    
    valid_data = throughput_summary.dropna(subset=['throughput_ops_per_sec'])
    if not valid_data.empty:
        bars = sns.barplot(data=valid_data, x='config', y='throughput_ops_per_sec')
        plt.title('Sustained Throughput by Configuration')
        plt.xlabel('Configuration')
        plt.ylabel('Throughput (ops/sec)')
        plt.xticks(rotation=45)
    
    plt.tight_layout()
    plt.savefig(f"{output_dir}/throughput_bar_plot.png", dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Created throughput_bar_plot.png")

def create_summary_table(df, output_dir):
    """Create a summary table of all results"""
    if df.empty:
        return
    
    # Group by config and test_type, get mean values
    agg_dict = {
        'total_ops': 'mean',
        'throughput_ops_per_sec': 'mean',
        'mean_latency_ns': 'mean',
        'p99_latency_ns': 'mean'
    }
    
    # Add hardware metrics if available
    hardware_cols = ['cpu_cycles_per_op', 'instructions_per_cycle', 'l1_cache_miss_rate', 
                    'l2_cache_miss_rate', 'l3_cache_miss_rate', 'memory_bandwidth_gb_per_sec', 
                    'branch_misprediction_rate']
    for col in hardware_cols:
        if col in df.columns:
            agg_dict[col] = 'mean'
    
    summary = df.groupby(['config', 'test_type']).agg(agg_dict).round(2)
    
    # Save as CSV
    summary.to_csv(f"{output_dir}/performance_summary.csv")
    print(f"Created performance_summary.csv")
    
    # Create a formatted table visualization
    fig, ax = plt.subplots(figsize=(14, 8))
    ax.axis('tight')
    ax.axis('off')
    
    # Create table data with N/A for missing latency data
    table_data = []
    for (config, test_type), row in summary.iterrows():
        mean_lat = "N/A" if pd.isna(row['mean_latency_ns']) else f"{row['mean_latency_ns']:.0f}"
        p99_lat = "N/A" if pd.isna(row['p99_latency_ns']) else f"{row['p99_latency_ns']:.0f}"
        
        table_row = [
            config,
            test_type,
            f"{row['total_ops']:.0f}",
            f"{row['throughput_ops_per_sec']:.0f}",
            mean_lat,
            p99_lat
        ]
        
        # Add hardware metrics if available
        if 'cpu_cycles_per_op' in row:
            table_row.append(f"{row['cpu_cycles_per_op']:.1f}")
        
        table_data.append(table_row)
    
    headers = ['Config', 'Test Type', 'Total Ops', 'Throughput (ops/s)', 'Mean Latency (ns)', 'P99 Latency (ns)']
    
    # Add hardware metric headers if available
    if 'cpu_cycles_per_op' in summary.columns:
        headers.append('CPU Cycles/Op')
    
    table = ax.table(cellText=table_data, colLabels=headers, cellLoc='center', loc='center')
    table.auto_set_font_size(False)
    table.set_fontsize(10)
    table.scale(1.2, 1.5)
    
    # Style the table
    for i in range(len(headers)):
        table[(0, i)].set_facecolor('#40466e')
        table[(0, i)].set_text_props(weight='bold', color='white')
    
    plt.title('Benchmark Performance Summary', fontsize=16, pad=20)
    plt.savefig(f"{output_dir}/summary_table.png", dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Created summary_table.png")

def create_hardware_metrics_charts(df, output_dir):
    """Create visualization charts for hardware performance metrics separated by test type"""
    if df.empty:
        return
    
    # Check if hardware metrics are available
    hw_cols = ['cpu_cycles_per_op', 'instructions_per_cycle', 'l1_cache_miss_rate', 
               'l2_cache_miss_rate', 'l3_cache_miss_rate', 'memory_bandwidth_gb_per_sec', 
               'branch_misprediction_rate']
    
    available_cols = [col for col in hw_cols if col in df.columns and not df[col].isna().all()]
    
    if not available_cols:
        print("No hardware metrics available for visualization")
        return
    
    # Create separate charts for each test type
    latency_data = df[df['test_type'] == 'latency'].copy()
    throughput_data = df[df['test_type'] == 'throughput'].copy()
    
    # Hardware metrics from latency test
    if not latency_data.empty:
        create_hardware_metrics_for_test_type(latency_data, "Latency Test", "latency_test_hardware_metrics", available_cols, output_dir)
    
    # Hardware metrics from throughput test  
    if not throughput_data.empty:
        create_hardware_metrics_for_test_type(throughput_data, "Throughput Test", "throughput_test_hardware_metrics", available_cols, output_dir)

def create_hardware_metrics_for_test_type(data, test_name, filename, available_cols, output_dir):
    """Create hardware metrics chart for a specific test type"""
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle(f'Hardware Performance Metrics - {test_name}', fontsize=16)
    
    if 'cpu_cycles_per_op' in available_cols:
        ax = axes[0, 0]
        cpu_data = data.dropna(subset=['cpu_cycles_per_op'])
        if not cpu_data.empty:
            sns.barplot(data=cpu_data, x='config', y='cpu_cycles_per_op', ax=ax)
            ax.set_title('CPU Cycles per Operation')
            ax.set_ylabel('Cycles/Operation')
            ax.tick_params(axis='x', rotation=45)
    
    if 'instructions_per_cycle' in available_cols:
        ax = axes[0, 1]
        ipc_data = data.dropna(subset=['instructions_per_cycle'])
        if not ipc_data.empty:
            sns.barplot(data=ipc_data, x='config', y='instructions_per_cycle', ax=ax)
            ax.set_title('Instructions per Cycle (IPC)')
            ax.set_ylabel('Instructions/Cycle')
            ax.tick_params(axis='x', rotation=45)
    
    if 'memory_bandwidth_gb_per_sec' in available_cols:
        ax = axes[1, 0]
        mem_data = data.dropna(subset=['memory_bandwidth_gb_per_sec'])
        if not mem_data.empty:
            sns.barplot(data=mem_data, x='config', y='memory_bandwidth_gb_per_sec', ax=ax)
            ax.set_title('Memory Bandwidth Utilization')
            ax.set_ylabel('Bandwidth (GB/s)')
            ax.tick_params(axis='x', rotation=45)
    
    if 'branch_misprediction_rate' in available_cols:
        ax = axes[1, 1]
        branch_data = data.dropna(subset=['branch_misprediction_rate'])
        if not branch_data.empty:
            sns.barplot(data=branch_data, x='config', y='branch_misprediction_rate', ax=ax)
            ax.set_title('Branch Misprediction Rate')
            ax.set_ylabel('Misprediction Rate (%)')
            ax.tick_params(axis='x', rotation=45)
    
    plt.tight_layout()
    plt.savefig(f"{output_dir}/{filename}.png", dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Created {filename}.png")

def create_hardware_summary_table(df, output_dir):
    """Create a comprehensive hardware metrics summary table"""
    if df.empty:
        return
    
    hw_cols = ['cpu_cycles_per_op', 'instructions_per_cycle', 'l1_cache_miss_rate', 
               'l2_cache_miss_rate', 'l3_cache_miss_rate', 'memory_bandwidth_gb_per_sec', 
               'branch_misprediction_rate']
    
    available_cols = [col for col in hw_cols if col in df.columns and not df[col].isna().all()]
    
    if not available_cols:
        print("No hardware metrics available for hardware summary table")
        return
    
    agg_dict = {col: 'mean' for col in available_cols}
    
    hardware_summary = df.groupby(['config', 'test_type']).agg(agg_dict).round(3)
    
    hardware_summary.to_csv(f"{output_dir}/hardware_summary.csv")
    print(f"Created hardware_summary.csv")
    
    fig, ax = plt.subplots(figsize=(18, 10))
    ax.axis('tight')
    ax.axis('off')
    
    table_data = []
    for (config, test_type), row in hardware_summary.iterrows():
        table_row = [config, test_type]
        
        if 'cpu_cycles_per_op' in row:
            table_row.append(f"{row['cpu_cycles_per_op']:.1f}")
        if 'instructions_per_cycle' in row:
            table_row.append(f"{row['instructions_per_cycle']:.3f}")
        if 'l1_cache_miss_rate' in row:
            table_row.append(f"{row['l1_cache_miss_rate']:.2f}%")
        if 'l2_cache_miss_rate' in row:
            table_row.append(f"{row['l2_cache_miss_rate']:.2f}%")
        if 'l3_cache_miss_rate' in row:
            table_row.append(f"{row['l3_cache_miss_rate']:.2f}%")
        if 'memory_bandwidth_gb_per_sec' in row:
            table_row.append(f"{row['memory_bandwidth_gb_per_sec']:.1f} GB/s")
        if 'branch_misprediction_rate' in row:
            table_row.append(f"{row['branch_misprediction_rate']:.3f}%")
        
        table_data.append(table_row)
    
    headers = ['Configuration', 'Test Type']
    if 'cpu_cycles_per_op' in hardware_summary.columns:
        headers.append('CPU Cycles/Op')
    if 'instructions_per_cycle' in hardware_summary.columns:
        headers.append('IPC')
    if 'l1_cache_miss_rate' in hardware_summary.columns:
        headers.append('L1 Miss %')
    if 'l2_cache_miss_rate' in hardware_summary.columns:
        headers.append('L2 Miss %')
    if 'l3_cache_miss_rate' in hardware_summary.columns:
        headers.append('L3 Miss %')
    if 'memory_bandwidth_gb_per_sec' in hardware_summary.columns:
        headers.append('Memory BW')
    if 'branch_misprediction_rate' in hardware_summary.columns:
        headers.append('Branch Miss %')
    
    table = ax.table(cellText=table_data, colLabels=headers, cellLoc='center', loc='center')
    table.auto_set_font_size(False)
    table.set_fontsize(9)
    table.scale(1.2, 1.8)
    
    for i in range(len(headers)):
        table[(0, i)].set_facecolor('#2E4057')
        table[(0, i)].set_text_props(weight='bold', color='white')
    
    for i, (config, test_type) in enumerate(hardware_summary.index):
        row_idx = i + 1  # +1 because row 0 is headers
        if test_type == 'latency':
            for j in range(len(headers)):
                table[(row_idx, j)].set_facecolor('#E8F4FD')
        else:  # throughput
            for j in range(len(headers)):
                table[(row_idx, j)].set_facecolor('#FFF2CC')
    
    plt.title('Hardware Performance Metrics Summary\n(Blue: Latency Test, Yellow: Throughput Test)', 
              fontsize=16, pad=20)
    plt.tight_layout()
    plt.savefig(f"{output_dir}/hardware_summary_table.png", dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Created hardware_summary_table.png")


def main():
    if len(sys.argv) < 2:
        print("Usage: python visualize_results.py <results_directory>")
        print("Example: python visualize_results.py benchmark/results")
        sys.exit(1)
    
    results_dir = sys.argv[1]
    
    if not os.path.exists(results_dir):
        print(f"Results directory not found: {results_dir}")
        sys.exit(1)
    
    print("SIMD-LOB Benchmark Results Analyzer")
    print("=" * 40)
    
    df = load_benchmark_results(results_dir)
    if df.empty:
        print("No benchmark data found to analyze")
        sys.exit(1)
    
    df = clean_data(df)
    if df.empty:
        print("No valid benchmark data after cleaning")
        sys.exit(1)
    
    print(f"\nData Summary:")
    print(f"  Configurations: {', '.join(df['config'].unique())}")
    print(f"  Test types: {', '.join(df['test_type'].unique())}")
    print(f"  Total records: {len(df)}")
    
    print(f"\nGenerating visualizations...")
    create_visualizations(df)
    
    print(f"\nAnalysis complete!")
    print(f"Visualizations saved to: visualizations/")
    print(f"Summary data saved to: visualizations/performance_summary.csv")

if __name__ == "__main__":
    main()
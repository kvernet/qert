"""
Statistical analysis pipeline for qert telemetry data.

Reads telemetry CSV files from a results directory, groups by experimental
condition, and tests the primary hypothesis:

    Does the cache miss rate saturation depth scale with Page time,
    and is the scaling coefficient alpha invariant across conditions?

Phase 1: since hardware counters are stubbed (zeros), this script focuses
on validating the analysis pipeline using entropy data and execution timing.

Usage:
    python scripts/analyze.py results/2026-05-17_12-00-00/
    python scripts/analyze.py results/2026-05-17_12-00-00/ --plot
    python scripts/analyze.py results/2026-05-17_12-00-00/ --output report.pdf
"""

import argparse
import json
import sys
from pathlib import Path

import numpy as np

# Optional: plotting requires matplotlib.
try:
    import matplotlib.pyplot as plt
    HAS_PLT = True
except ImportError:
    HAS_PLT = False


# ============================================================================
# Data Loading
# ============================================================================

def parse_telemetry_csv(filepath: str) -> tuple[dict, np.ndarray]:
    """
    Parse a qert telemetry CSV file.
    
    Returns:
        metadata: dict of experiment metadata (from the JSON comment line).
        data: structured numpy array with columns matching the telemetry schema.
    """
    with open(filepath, "r") as f:
        # Line 1: metadata JSON comment.
        first_line = f.readline().strip()
        
        if first_line.startswith("# "):
            metadata = json.loads(first_line[2:])
        else:
            raise ValueError(f"Missing metadata comment in {filepath}")
        
        # Line 2: CSV header.
        header = f.readline().strip().split(",")
        
        # Remaining lines: data.
        rows = []
        for line in f:
            line = line.strip()
            if not line:
                continue
            values = line.split(",")
            row = []
            for val in values:
                if val == "nan":
                    row.append(np.nan)
                else:
                    try:
                        row.append(float(val))
                    except ValueError:
                        row.append(val)
            rows.append(tuple(row))
    
    # Build structured array.
    dtype = [
        ("event_id", "u8"),
        ("depth", "u4"),
        ("gate_idx", "u4"),
        ("execution_time_ns", "u8"),
        ("l3_misses_delta", "u8"),
        ("tlb_misses_delta", "u8"),
        ("working_set_kb", "u8"),
        ("stride_entropy", "f8"),
        ("half_chain_entropy", "f8"),
    ]
    
    data = np.array(rows, dtype=dtype)
    return metadata, data


def load_results_directory(results_dir: str) -> list[dict]:
    """
    Load all CSV files from a results directory.
    
    Returns a list of dicts: {"metadata": ..., "data": ..., "filename": ...}
    """
    results_path = Path(results_dir)
    csv_files = sorted(results_path.glob("*.csv"))
    
    if not csv_files:
        raise FileNotFoundError(f"No CSV files found in {results_dir}")
    
    runs = []
    for csv_file in csv_files:
        try:
            metadata, data = parse_telemetry_csv(str(csv_file))
            runs.append({
                "metadata": metadata,
                "data": data,
                "filename": csv_file.name,
            })
        except Exception as e:
            print(f"Warning: skipping {csv_file.name}: {e}", file=sys.stderr)
    
    return runs


# ============================================================================
# Data Extraction
# ============================================================================

def extract_entropy_by_depth(data: np.ndarray) -> dict[int, float]:
    """
    Extract half-chain entropy at each depth where it was sampled.
    
    Returns dict: depth -> entropy value.
    Entropy is only recorded at the last gate of even layers.
    """
    entropy_by_depth = {}
    
    for row in data:
        depth = int(row["depth"])
        entropy = row["half_chain_entropy"]
        
        if not np.isnan(entropy):
            entropy_by_depth[depth] = entropy
    
    return entropy_by_depth


def extract_timing_by_depth(data: np.ndarray) -> dict[int, dict[str, float]]:
    """
    Extract aggregate timing statistics per circuit depth layer.
    
    Returns dict: depth -> {"sum_ns", "mean_ns", "std_ns", "count"}.
    """
    depths = {}
    
    for row in data:
        depth = int(row["depth"])
        t = row["execution_time_ns"]
        
        if depth not in depths:
            depths[depth] = []
        depths[depth].append(t)
    
    result = {}
    for depth, times in depths.items():
        arr = np.array(times)
        result[depth] = {
            "sum_ns": float(np.sum(arr)),
            "mean_ns": float(np.mean(arr)),
            "std_ns": float(np.std(arr)),
            "count": len(times),
        }
    
    return result


# ============================================================================
# Saturation Analysis
# ============================================================================

def fit_saturation_curve(
    depths: np.ndarray,
    values: np.ndarray,
) -> dict:
    """
    Fit a piecewise linear model (growth + plateau) to find saturation depth.
    
    Returns:
        dict with keys: d50, d90, inflection_depth, r_squared, fit_quality
    """
    n = len(depths)
    if n < 4:
        return {"d50": np.nan, "d90": np.nan, "inflection_depth": np.nan,
                "r_squared": np.nan, "fit_quality": "insufficient_data"}
    
    # Try all possible split points.
    best_r2 = -1
    best_split = None
    
    for split_idx in range(2, n - 1):
        # Segment 1: growth (depths 0..split_idx-1)
        x1 = depths[:split_idx]
        y1 = values[:split_idx]
        
        # Segment 2: plateau (depths split_idx..n-1)
        x2 = depths[split_idx:]
        y2 = values[split_idx:]
        
        if len(x1) < 2 or len(x2) < 2:
            continue
        
        # Linear fit for segment 1.
        coeffs1 = np.polyfit(x1, y1, 1)
        y1_pred = np.polyval(coeffs1, x1)
        
        # Constant fit for segment 2 (plateau).
        y2_pred = np.full_like(y2, np.mean(y2))
        
        # Total R².
        ss_res = np.sum((y1 - y1_pred) ** 2) + np.sum((y2 - y2_pred) ** 2)
        ss_tot = np.sum((values - np.mean(values)) ** 2)
        
        if ss_tot < 1e-15:
            continue
        
        r2 = 1.0 - ss_res / ss_tot
        
        if r2 > best_r2:
            best_r2 = r2
            best_split = split_idx
    
    if best_split is None:
        return {"d50": np.nan, "d90": np.nan, "inflection_depth": np.nan,
                "r_squared": np.nan, "fit_quality": "fit_failed"}
    
    # Compute saturation value (mean of plateau segment).
    plateau_mean = np.mean(values[best_split:])
    
    # Compute D_50 and D_90: depths where value reaches 50% and 90% of plateau.
    half_val = plateau_mean * 0.5
    ninety_val = plateau_mean * 0.9
    
    d50 = interpolate_depth(depths, values, half_val)
    d90 = interpolate_depth(depths, values, ninety_val)
    
    # Inflection: the split point depth.
    inflection_depth = float(depths[best_split])
    
    return {
        "d50": d50,
        "d90": d90,
        "inflection_depth": inflection_depth,
        "plateau_value": float(plateau_mean),
        "r_squared": best_r2,
        "fit_quality": "good" if best_r2 > 0.8 else "poor",
    }


def interpolate_depth(
    depths: np.ndarray,
    values: np.ndarray,
    target: float,
) -> float:
    """Find the depth at which values crosses target by linear interpolation."""
    if target <= values[0]:
        return float(depths[0])
    if target >= values[-1]:
        return float(depths[-1])
    
    for i in range(len(depths) - 1):
        if values[i] <= target <= values[i + 1]:
            frac = (target - values[i]) / (values[i + 1] - values[i])
            return float(depths[i] + frac * (depths[i + 1] - depths[i]))
    
    return float(depths[-1])


# ============================================================================
# Hypothesis Testing
# ============================================================================

def test_invariant_hypothesis(
    runs: list[dict],
) -> dict:
    """
    Test the primary invariant hypothesis.
    
    For each (N, mapping) condition, fit saturation curves to entropy vs depth.
    Then test whether D_90 scales as alpha * D_Page + β with alpha invariant across conditions.
    
    Returns summary statistics.
    """
    # Group runs by condition.
    conditions = {}
    for run in runs:
        meta = run["metadata"]
        n = meta["num_qubits"]
        mapping = meta["qubit_mapping"]
        key = (n, mapping)
        
        if key not in conditions:
            conditions[key] = []
        conditions[key].append(run)
    
    results = []
    
    for (n, mapping), cond_runs in conditions.items():
        # Aggregate entropy across seeds for this condition.
        # For each depth, collect entropy values from all seeds.
        depth_entropy = {}
        
        for run in cond_runs:
            ent_by_depth = extract_entropy_by_depth(run["data"])
            for depth, entropy in ent_by_depth.items():
                if depth not in depth_entropy:
                    depth_entropy[depth] = []
                depth_entropy[depth].append(entropy)
        
        # Compute mean entropy per depth.
        depths = np.array(sorted(depth_entropy.keys()))
        mean_entropy = np.array([np.mean(depth_entropy[d]) for d in depths])
        std_entropy = np.array([np.std(depth_entropy[d]) for d in depths])
        
        # Fit saturation curve.
        fit = fit_saturation_curve(depths, mean_entropy)
        d_page = n / 2.0  # Theoretical Page time.
        
        results.append({
            "num_qubits": n,
            "mapping": mapping,
            "num_seeds": len(cond_runs),
            "d_page": d_page,
            "d50": fit["d50"],
            "d90": fit["d90"],
            "inflection_depth": fit["inflection_depth"],
            "plateau_value": fit["plateau_value"],
            "r_squared": fit["r_squared"],
            "fit_quality": fit["fit_quality"],
            "alpha_d90": fit["d90"] / d_page if d_page > 0 else np.nan,
            "mean_entropy_curve": list(mean_entropy),
            "std_entropy_curve": list(std_entropy),
            "depths": list(depths),
        })
    
    return results


# ============================================================================
# Reporting
# ============================================================================

def print_summary(hypothesis_results: list[dict]):
    """Print a summary table of hypothesis test results."""
    print("\n" + "=" * 80)
    print("HYPOTHESIS TEST SUMMARY")
    print("=" * 80)
    print(f"{'N':>4s}  {'Mapping':<16s}  {'Seeds':>5s}  "
          f"{'D_Page':>8s}  {'D_90':>8s}  {'α=D90/Page':>10s}  "
          f"{'R²':>6s}  {'Quality':<8s}")
    print("-" * 80)
    
    for r in sorted(hypothesis_results, key=lambda x: (x["num_qubits"], x["mapping"])):
        print(
            f"{r['num_qubits']:4d}  {r['mapping']:<16s}  {r['num_seeds']:5d}  "
            f"{r['d_page']:8.1f}  {r['d90']:8.1f}  {r['alpha_d90']:10.3f}  "
            f"{r['r_squared']:6.3f}  {r['fit_quality']:<8s}"
        )
    
    # Check invariant: α should be stable across conditions with the same N.
    print("\n--- Invariant Test ---")
    by_n = {}
    for r in hypothesis_results:
        n = r["num_qubits"]
        if n not in by_n:
            by_n[n] = []
        by_n[n].append(r["alpha_d90"])
    
    for n, alphas in sorted(by_n.items()):
        alphas = [a for a in alphas if not np.isnan(a)]
        if len(alphas) >= 2:
            spread = max(alphas) - min(alphas)
            status = "PASS" if spread < 0.2 else "FAIL"
            print(f"  N={n:2d}: α ∈ [{min(alphas):.3f}, {max(alphas):.3f}], "
                  f"spread={spread:.3f} ({status})")
        else:
            print(f"  N={n:2d}: insufficient mappings for comparison")


# ============================================================================
# Plotting
# ============================================================================

def plot_entropy_curves(
    hypothesis_results: list[dict],
    output_path: str | None = None,
):
    """
    Entropy growth curves for all system sizes.
    
    Grid layout: 2 rows x 4 columns for N=4-18 (8 panels).
    Each panel shows mean entropy vs depth with ±1σ error bars,
    Page time marker, and saturation fit annotation.
    """
    if not HAS_PLT:
        print("Matplotlib not installed. Skipping plots.")
        return

    # Group by N, sort.
    by_n = {}
    for r in hypothesis_results:
        n = r["num_qubits"]
        if n not in by_n:
            by_n[n] = []
        by_n[n].append(r)

    n_list = sorted(by_n.keys())
    num_n = len(n_list)

    # Grid: 2 rows, ceil(N/2) columns.
    ncols = (num_n + 1) // 2
    nrows = 2 if num_n > 1 else 1

    fig, axes = plt.subplots(
        nrows, ncols,
        figsize=(4.5 * ncols, 4.0 * nrows),
        squeeze=False,
    )

    # Flatten for easy iteration; hide unused subplots.
    axes_flat = axes.flatten()
    for idx in range(num_n, len(axes_flat)):
        axes_flat[idx].set_visible(False)

    for idx, n in enumerate(n_list):
        ax = axes_flat[idx]
        results = by_n[n]

        # Plot one curve per mapping (they overlap perfectly, so plot
        # only the first mapping to avoid visual clutter).
        r = results[0]
        depths = r["depths"]
        means = r["mean_entropy_curve"]
        stds = r["std_entropy_curve"]

        ax.errorbar(
            depths, means, yerr=stds,
            marker="o", markersize=4,
            capsize=3, linewidth=1.2,
            color="#1f77b4", label=f"N={n}",
        )

        # Page time marker.
        d_page = n / 2.0
        ax.axvline(
            x=d_page, color="red", linestyle="--",
            alpha=0.6, linewidth=1.0,
        )

        # Annotate with α value.
        alpha_val = r["alpha_d90"]
        ax.text(
            0.95, 0.05,
            fr"$\alpha$ = {alpha_val:.3f}",
            transform=ax.transAxes,
            fontsize=9,
            ha="right",
            va="bottom",
            bbox=dict(boxstyle="round,pad=0.3", facecolor="white", alpha=0.8),
        )

        # Theoretical maximum entropy line.
        k = n // 2
        s_max = k * np.log(2)
        ax.axhline(
            y=s_max, color="gray", linestyle=":",
            alpha=0.5, linewidth=0.8,
        )

        ax.set_xlabel("Circuit depth", fontsize=10)
        ax.set_ylabel("Half-chain entropy (nats)", fontsize=10)
        ax.set_title(f"N = {n}", fontsize=11, fontweight="bold")
        ax.grid(True, alpha=0.3)
        ax.set_xlim(left=0)

    # Shared legend below the plots.
    num_seeds = hypothesis_results[0]["num_seeds"]
    fig.text(
        0.5, 0.01,
        f"Solid line: mean entropy across {num_seeds} instances. "
        r"Error bars: $\pm 1\alpha$. "
        "Red dashed: Page time (N/2). "
        "Gray dotted: maximal entropy k·ln(2).",
        ha="center", fontsize=9, fontstyle="italic",
    )

    fig.suptitle(
        "Entanglement entropy growth in 1D Brickwall circuits",
        fontsize=14, fontweight="bold", y=1.01,
    )
    plt.tight_layout(rect=[0, 0.04, 1, 0.96])

    if output_path:
        plt.savefig(output_path, dpi=200, bbox_inches="tight")
        print(f"Entropy curves saved to {output_path}")
    else:
        plt.show()
    plt.close(fig)


def plot_alpha_summary(
    hypothesis_results: list[dict],
    output_path: str | None = None,
):
    """
    Scaling coefficient alpha vs system size N.
    
    Single panel showing alpha = D_90 / (N/2) for each N.
    Horizontal line at alpha = 1.0 (Page prediction).
    Horizontal band for the measured mean alpha.
    """
    if not HAS_PLT:
        print("Matplotlib not installed. Skipping plots.")
        return

    # Extract one row per N (all mappings identical, so take first).
    by_n = {}
    for r in hypothesis_results:
        n = r["num_qubits"]
        if n not in by_n:
            by_n[n] = r["alpha_d90"]

    n_list = sorted(by_n.keys())
    alphas = [by_n[n] for n in n_list]

    mean_alpha = np.mean(alphas)
    std_alpha = np.std(alphas)

    fig, ax = plt.subplots(figsize=(8, 4.5))

    # α values.
    ax.plot(
        n_list, alphas,
        marker="o", markersize=8,
        linewidth=1.5, color="#1f77b4",
        label=fr"Measured $\alpha$ (mean = {mean_alpha:.3f} ± {std_alpha:.3f})",
    )

    # Page prediction α = 1.0.
    ax.axhline(
        y=1.0, color="red", linestyle="--",
        linewidth=1.2, alpha=0.7,
        label=r"Page prediction ($\alpha$ = 1.0)",
    )

    # Mean α band.
    ax.axhspan(
        mean_alpha - std_alpha, mean_alpha + std_alpha,
        alpha=0.12, color="#1f77b4",
        label=fr"Mean $\alpha$ band ($\pm 1\alpha$)",
    )

    ax.set_xlabel("System size", fontsize=12)
    ax.set_ylabel(r"Scaling coefficient $\alpha$ = D$_{90}$ / (N/2)", fontsize=12)
    ax.set_title(
        "Entanglement saturation scaling coefficient",
        fontsize=13, fontweight="bold",
    )
    ax.legend(fontsize=10, loc="lower right")
    ax.grid(True, alpha=0.3)
    ax.set_xlim(n_list[0] - 0.5, n_list[-1] + 0.5)
    ax.set_ylim(0.8, 2.2)

    # Annotate key finding.
    ax.text(
        0.98, 0.95,
        f"Saturation occurs at\n{mean_alpha:.2f} x the Page time\n"
        f"across N = {n_list[0]} - {n_list[-1]}",
        transform=ax.transAxes,
        fontsize=10,
        ha="right",
        va="top",
        bbox=dict(boxstyle="round,pad=0.4", facecolor="lightyellow", alpha=0.9),
    )

    plt.tight_layout()

    if output_path:
        plt.savefig(output_path, dpi=200, bbox_inches="tight")
        print(f"Alpha summary saved to {output_path}")
    else:
        plt.show()
    plt.close(fig)


def plot_execution_time(
    runs: list[dict],
    output_path: str | None = None,
):
    """
    Mean per-gate execution time vs circuit depth.
    
    Extracts timing data directly from telemetry CSVs (not from
    hypothesis results, which aggregate only entropy).
    """
    if not HAS_PLT:
        print("Matplotlib not installed. Skipping plots.")
        return

    # Aggregate execution time by (N, depth) across all runs.
    timing = {}  # key: (N, depth) -> list of mean times

    for run in runs:
        n = run["metadata"]["num_qubits"]
        if n not in [4, 8, 12, 16]:  # Subset for readability
            continue

        data = run["data"]
        for row in data:
            depth = int(row["depth"])
            t = row["execution_time_ns"]
            key = (n, depth)
            if key not in timing:
                timing[key] = []
            timing[key].append(t)

    # Compute mean per (N, depth).
    summary = {}
    for (n, depth), times in timing.items():
        arr = np.array(times)
        summary[(n, depth)] = {
            "mean_ns": np.mean(arr),
            "std_ns": np.std(arr),
        }

    fig, ax = plt.subplots(figsize=(8, 4.5))

    colors = {4: "#1f77b4", 8: "#ff7f0e", 12: "#2ca02c", 16: "#d62728"}

    for n in [4, 8, 12, 16]:
        depths = sorted([d for (nn, d) in summary if nn == n])
        means = [summary[(n, d)]["mean_ns"] / 1000.0 for d in depths]  # ns -> µs
        stds = [summary[(n, d)]["std_ns"] / 1000.0 for d in depths]

        ax.errorbar(
            depths, means, yerr=stds,
            marker="o", markersize=5,
            capsize=3, linewidth=1.2,
            color=colors[n],
            label=f"N={n}",
        )

    ax.set_xlabel("Circuit depth", fontsize=12)
    ax.set_ylabel("Per-gate execution time (µs)", fontsize=12)
    ax.set_title(
        "Gate execution time vs. circuit depth",
        fontsize=13, fontweight="bold",
    )
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.3)
    ax.set_xlim(left=0)

    # Annotate: timing is constant across depth.
    #ax.text(
    #    0.98, 0.95,
    #    "Execution time is approximately\n"
    #    "constant across circuit depth\n"
    #    "for each N (statevector fits in cache).",
    #    transform=ax.transAxes,
    #    fontsize=9,
    #    ha="right",
    #    va="top",
    #    bbox=dict(boxstyle="round,pad=0.4", facecolor="lightyellow", alpha=0.9),
    #)

    plt.tight_layout()

    if output_path:
        plt.savefig(output_path, dpi=200, bbox_inches="tight")
        print(f"Execution time saved to {output_path}")
    else:
        plt.show()
    plt.close(fig)


def plot_all(
    hypothesis_results: list[dict],
    runs: list[dict],
    output_dir: str | None = None,
):
    """Generate all three figures."""
    if not output_dir:
        return
    
    plot_entropy_curves(hypothesis_results, output_dir + "/entropy.pdf")
    plot_alpha_summary(hypothesis_results, output_dir + "/alpha.pdf")
    plot_execution_time(runs, output_dir + "/timing.pdf")

# ============================================================================
# Main
# ============================================================================

def parse_args():
    parser = argparse.ArgumentParser(description="qert telemetry analysis")
    parser.add_argument("results_dir", type=str,
                        help="Path to results directory containing CSV files")
    parser.add_argument("--plot", action="store_true",
                        help="Show plots (requires matplotlib)")
    parser.add_argument("--output", type=str, default=None,
                        help="Output dir to save plots (e.g., results/figures)")
    parser.add_argument("--json", type=str, default=None,
                        help="Export analysis results to JSON")
    return parser.parse_args()


def main():
    args = parse_args()
    
    # Load data.
    print(f"Loading results from: {args.results_dir}")
    runs = load_results_directory(args.results_dir)
    print(f"Loaded {len(runs)} runs.")
    
    if not runs:
        print("No valid runs found.")
        sys.exit(1)
    
    # Basic statistics.
    total_events = sum(len(r["data"]) for r in runs)
    unique_n = set(r["metadata"]["num_qubits"] for r in runs)
    unique_mappings = set(r["metadata"]["qubit_mapping"] for r in runs)
    
    print(f"\nTotal telemetry events: {total_events:,}")
    print(f"System sizes: {sorted(unique_n)}")
    print(f"Mappings: {sorted(unique_mappings)}")
    
    # Run hypothesis tests.
    print("\nRunning hypothesis tests...")
    results = test_invariant_hypothesis(runs)
    print_summary(results)
    
    # Export JSON if requested.
    if args.json:
        with open(args.json, "w") as f:
            json.dump(results, f, indent=2, default=str)
        print(f"\nAnalysis results exported to {args.json}")
    
    # Plot if requested.
    if args.plot or args.output:
        path = Path(args.output)
        path.mkdir(parents=True, exist_ok=True)
        plot_all(results, runs, args.output)


if __name__ == "__main__":
    main()
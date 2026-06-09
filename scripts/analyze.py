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
    python scripts/analyze.py results/2026-05-17_12-00-00/ --output figures/
"""

import argparse
import json
import math
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
            entropy_by_depth[depth] = float(entropy)
    
    return entropy_by_depth

def extract_cache_by_depth(data: np.ndarray) -> dict[int, list[float]]:
    """
    Extract per-gate L3 cache misses at each depth.
    Returns dict: depth -> list of miss counts (one per gate in that layer).
    """
    cache_by_depth = {}
    for row in data:
        depth = int(row["depth"])
        l3 = row["l3_misses_delta"]
        if depth not in cache_by_depth:
            cache_by_depth[depth] = []
        cache_by_depth[depth].append(l3)
    return cache_by_depth

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
        dict with keys: d50, d90, inflection_depth, plateau_value, r_squared, fit_quality
    """
    n = len(depths)
    if n < 4:
        return {
            "d50": np.nan, "d90": np.nan, "inflection_depth": np.nan,
            "plateau_value": np.nan, "r_squared": np.nan,
            "fit_quality": "insufficient_data"
        }
    
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
        return {
            "d50": np.nan, "d90": np.nan, "inflection_depth": np.nan,
            "plateau_value": np.nan, "r_squared": np.nan,
            "fit_quality": "fit_failed"
        }
    
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
) -> list[dict]:
    """
    Test the primary invariant hypothesis.
    
    For each (N, mapping) condition, aggregate entropy across seeds,
    fit saturation curves to entropy vs depth, and test whether D_90
    scales as alpha * D_Page + beta with alpha invariant across mappings.
    """
    # Group runs by (N, mapping).
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
        # Each run produces entropy at every even layer.
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
            "r_squared": float(fit["r_squared"]),
            "fit_quality": fit["fit_quality"],
            "alpha_d90": fit["d90"] / d_page if d_page > 0 else np.nan,
            "mean_entropy_curve": [float(v) for v in mean_entropy],
            "std_entropy_curve": [float(v) for v in std_entropy],
            "depths": [int(d) for d in depths],
        })
    
    return results

def test_cache_hypothesis(runs: list[dict]) -> list[dict]:
    """
    Test the cache-entanglement hypothesis.
    
    For each (N, mapping) condition, aggregate L3 cache misses across seeds,
    fit saturation curves, and compare α_cache to α_entropy.
    """
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
        # Aggregate mean L3 misses per gate at each depth.
        depth_l3 = {}
        for run in cond_runs:
            cache_by_depth = extract_cache_by_depth(run["data"])
            for depth, misses in cache_by_depth.items():
                if depth not in depth_l3:
                    depth_l3[depth] = []
                depth_l3[depth].extend(misses)
        
        depths = np.array(sorted(depth_l3.keys()))
        mean_l3 = np.array([np.mean(depth_l3[d]) for d in depths])
        std_l3 = np.array([np.std(depth_l3[d]) for d in depths])
        
        # Fit saturation curve to L3 misses.
        fit = fit_saturation_curve(depths, mean_l3)
        d_page = n / 2.0
        
        results.append({
            "num_qubits": n,
            "mapping": mapping,
            "num_seeds": len(cond_runs),
            "d_page": d_page,
            "d90_cache": fit["d90"],
            "alpha_cache": fit["d90"] / d_page if d_page > 0 else np.nan,
            "r_squared_cache": float(fit["r_squared"]),
            "fit_quality_cache": fit["fit_quality"],
            "mean_l3_curve": [float(v) for v in mean_l3],
            "std_l3_curve": [float(v) for v in std_l3],
            "depths": [int(d) for d in depths],
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
    
    # Check invariant: α should be stable across mappings for each N.
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

def print_cache_summary(cache_results: list[dict]):
    """Print cache hypothesis test results."""
    print("\n" + "=" * 80)
    print("CACHE HYPOTHESIS TEST SUMMARY")
    print("=" * 80)
    print(f"{'N':>4s}  {'Mapping':<16s}  {'Seeds':>5s}  "
          f"{'D_Page':>8s}  {'D_90_cache':>10s}  {'α_cache':>10s}  "
          f"{'R²':>6s}  {'Quality':<8s}")
    print("-" * 80)
    
    for r in sorted(cache_results, key=lambda x: (x["num_qubits"], x["mapping"])):
        print(
            f"{r['num_qubits']:4d}  {r['mapping']:<16s}  {r['num_seeds']:5d}  "
            f"{r['d_page']:8.1f}  {r['d90_cache']:10.1f}  {r['alpha_cache']:10.3f}  "
            f"{r['r_squared_cache']:6.3f}  {r['fit_quality_cache']:<8s}"
        )
    
    print("\n--- Cache Invariant Test ---")
    by_n = {}
    for r in cache_results:
        n = r["num_qubits"]
        if n not in by_n:
            by_n[n] = []
        by_n[n].append(r["alpha_cache"])
    
    for n, alphas in sorted(by_n.items()):
        alphas = [a for a in alphas if not np.isnan(a)]
        if len(alphas) >= 2:
            spread = max(alphas) - min(alphas)
            status = "PASS" if spread < 0.2 else "FAIL"
            print(f"  N={n:2d}: α_cache ∈ [{min(alphas):.3f}, {max(alphas):.3f}], "
                  f"spread={spread:.3f} ({status})")

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
    Each panel shows mean entropy vs depth for all three mappings.
    """
    if not HAS_PLT:
        print("Matplotlib not installed. Skipping plots.")
        return

    by_n = {}
    for r in hypothesis_results:
        n = r["num_qubits"]
        if n not in by_n:
            by_n[n] = []
        by_n[n].append(r)

    n_list = sorted(by_n.keys())
    num_n = len(n_list)

    ncols = (num_n + 1) // 2
    nrows = 2 if num_n > 1 else 1

    fig, axes = plt.subplots(
        nrows, ncols,
        figsize=(4.5 * ncols, 4.0 * nrows),
        squeeze=False,
    )

    axes_flat = axes.flatten()
    for idx in range(num_n, len(axes_flat)):
        axes_flat[idx].set_visible(False)

    colors = {"lexicographic": "#1f77b4", "gray": "#ff7f0e", "locality_aware": "#2ca02c"}

    for idx, n in enumerate(n_list):
        ax = axes_flat[idx]
        results = by_n[n]

        for r in results:
            mapping = r["mapping"]
            depths = r["depths"]
            means = r["mean_entropy_curve"]
            stds = r["std_entropy_curve"]

            ax.errorbar(
                depths, means, yerr=stds,
                marker="o", markersize=3,
                capsize=2, linewidth=1.0,
                color=colors.get(mapping, "#333333"),
                label=fr"{mapping} ($\alpha$={r['alpha_d90']:.3f})",
                alpha=0.85,
            )

        d_page = r["d_page"]
        ax.axvline(
            x=d_page, color="red", linestyle="--",
            alpha=0.5, linewidth=0.8,
        )

        k = n // 2
        s_max = k * math.log(2)
        ax.axhline(
            y=s_max, color="gray", linestyle=":",
            alpha=0.4, linewidth=0.8,
        )

        ax.set_xlabel("Circuit depth", fontsize=9)
        ax.set_ylabel("Half-chain entropy (nats)", fontsize=9)
        ax.set_title(f"N = {n}", fontsize=10, fontweight="bold")
        ax.legend(fontsize=6, loc="lower right")
        ax.grid(True, alpha=0.3)
        ax.set_xlim(left=0)

    num_seeds = hypothesis_results[0]["num_seeds"]
    fig.text(
        0.5, 0.01,
        f"Solid lines: mean entropy across {num_seeds} instances. "
        r"Error bars: $\pm 1\sigma$. "
        "Red dashed: Page time (N/2). "
        "Gray dotted: maximal entropy k·ln(2).",
        ha="center", fontsize=9, fontstyle="italic",
    )

    fig.suptitle(
        "Entanglement entropy growth in 1D brickwall circuits",
        fontsize=14, fontweight="bold", y=1.01,
    )
    plt.tight_layout(rect=[0, 0.04, 1, 0.96])

    if output_path:
        plt.savefig(output_path, dpi=200, bbox_inches="tight")
        print(f"Entropy curves saved to {output_path}")
    else:
        plt.show()
    plt.close(fig)


def plot_alpha_summary(hypothesis_results: list[dict], output_path: str | None = None):
    """Scaling coefficient alpha vs system size N."""
    if not HAS_PLT:
        print("Matplotlib not installed. Skipping plots.")
        return

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

    ax.plot(
        n_list, alphas,
        marker="o", markersize=8,
        linewidth=1.5, color="#1f77b4",
        label=fr"Measured $\alpha$ (mean = {mean_alpha:.3f} $\pm$ {std_alpha:.3f})",
    )

    ax.axhline(
        y=1.0, color="red", linestyle="--",
        linewidth=1.2, alpha=0.7,
        label=r"Page prediction ($\alpha$ = 1.0)",
    )

    ax.axhspan(
        mean_alpha - std_alpha, mean_alpha + std_alpha,
        alpha=0.12, color="#1f77b4",
        label=fr"Mean $\alpha$ band ($\pm 1\sigma$)",
    )

    ax.set_xlabel("System size N", fontsize=12)
    ax.set_ylabel(r"Scaling coefficient $\alpha = D_{90} / (N/2)$", fontsize=12)
    ax.set_title(
        "Entanglement saturation scaling coefficient",
        fontsize=13, fontweight="bold",
    )
    ax.legend(fontsize=10, loc="lower right")
    ax.grid(True, alpha=0.3)
    ax.set_xlim(n_list[0] - 0.5, n_list[-1] + 0.5)

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
    Normalized per-gate execution time vs circuit depth.
    Excludes depth 0 (cold-start artifact).
    """
    if not HAS_PLT:
        print("Matplotlib not installed. Skipping plots.")
        return

    timing = {}

    for run in runs:
        n = run["metadata"]["num_qubits"]

        norm_factor = 2.0 ** n

        data = run["data"]
        for row in data:
            depth = int(row["depth"])
            if depth == 0:
                continue  # Skip cold-start layer
            t = row["execution_time_ns"]
            key = (n, depth)
            if key not in timing:
                timing[key] = []
            timing[key].append(t / norm_factor)

    summary = {}
    for (n, depth), times in timing.items():
        arr = np.array(times)
        summary[(n, depth)] = {
            "mean_ns": np.mean(arr),
            "std_ns": np.std(arr),
        }

    fig, ax = plt.subplots(figsize=(9, 5))

    cmap = plt.cm.viridis
    n_list = sorted(set(n for (n, d) in summary))
    if len(n_list) <= 1:
        return
    colors = {n: cmap(i / (len(n_list) - 1)) for i, n in enumerate(n_list)}

    for n in n_list:
        depths = sorted([d for (nn, d) in summary if nn == n])
        means = [summary[(n, d)]["mean_ns"] for d in depths]
        stds = [summary[(n, d)]["std_ns"] for d in depths]

        ax.errorbar(
            depths, means, yerr=stds,
            marker="o", markersize=3,
            capsize=2, linewidth=1.0,
            color=colors[n],
            label=f"N={n}",
            alpha=0.85,
        )

    ax.set_xlabel("Circuit depth", fontsize=12)
    ax.set_ylabel("Per-gate time / 2$^N$ (ns)", fontsize=12)
    ax.set_title(
        "Normalized execution time vs. circuit depth\n(depth 0 excluded: cold-start artifact)",
        fontsize=13, fontweight="bold",
    )
    ax.legend(fontsize=9, ncol=2)
    ax.grid(True, alpha=0.3)
    ax.set_xlim(left=1)

    plt.tight_layout()

    if output_path:
        plt.savefig(output_path, dpi=200, bbox_inches="tight")
        print(f"Normalized execution time saved to {output_path}")
    else:
        plt.show()
    plt.close(fig)


def plot_cache_curves(
    cache_results: list[dict],
    output_path: str | None = None,
):
    """L3 cache miss curves for all system sizes."""
    if not HAS_PLT:
        return

    by_n = {}
    for r in cache_results:
        n = r["num_qubits"]
        if n not in by_n:
            by_n[n] = []
        by_n[n].append(r)

    n_list = sorted(by_n.keys())
    num_n = len(n_list)
    ncols = (num_n + 1) // 2
    nrows = 2 if num_n > 1 else 1

    fig, axes = plt.subplots(nrows, ncols, figsize=(4.5 * ncols, 4.0 * nrows), squeeze=False)
    axes_flat = axes.flatten()
    for idx in range(num_n, len(axes_flat)):
        axes_flat[idx].set_visible(False)

    colors = {"lexicographic": "#1f77b4", "gray": "#ff7f0e", "locality_aware": "#2ca02c"}
    ylims = {16:[-40, 50], 18:[-200, 350], 20:[-1500, 3000]}
    for idx, n in enumerate(n_list):
        ax = axes_flat[idx]
        for r in by_n[n]:
            depths = r["depths"]
            means = r["mean_l3_curve"]
            stds = r["std_l3_curve"]
            mapping = r["mapping"]
            ax.errorbar(
                depths, means, yerr=stds,
                marker="o", markersize=3, capsize=2, linewidth=1.0,
                color=colors.get(mapping, "#333333"),
                label=fr"{mapping} ($\alpha$={r['alpha_cache']:.3f})",
                alpha=0.85,
            )

        ax.axvline(x=n/2, color="red", linestyle="--", alpha=0.5, linewidth=0.8)
        ax.set_xlabel("Circuit depth", fontsize=9)
        ax.set_ylabel("L3 misses / gate", fontsize=9)
        ax.set_title(f"N = {n}", fontsize=10, fontweight="bold")
        ax.legend(fontsize=6, loc="upper right")
        ax.grid(True, alpha=0.3)
        ylim = ylims[n] if n in ylims else None
        ax.set_ylim(ylim)

    fig.suptitle("L3 Cache Misses vs. Circuit Depth", fontsize=14, fontweight="bold", y=1.01)
    plt.tight_layout(rect=[0, 0.04, 1, 0.96])

    if output_path:
        plt.savefig(output_path, dpi=200, bbox_inches="tight")
        print(f"Cache curves saved to {output_path}")
    else:
        plt.show()
    plt.close(fig)


# ============================================================================
# Main
# ============================================================================

def parse_args():
    parser = argparse.ArgumentParser(description="qert telemetry analysis")
    parser.add_argument(
        "results_dir", type=str, default="csv",
        help="Path to results directory containing CSV files"
    )
    parser.add_argument(
        "--plot", action="store_true",
        help="Show plots interactively"
    )
    parser.add_argument(
        "--output", type=str, default=None,
        help="Output directory to save plots (e.g., figures/)"
    )
    parser.add_argument(
        "--json", type=str, default=None,
        help="Export analysis results to JSON"
    )
    return parser.parse_args()


def main():
    args = parse_args()
    
    print(f"Loading results from: {args.results_dir}")
    runs = load_results_directory(args.results_dir)
    print(f"Loaded {len(runs)} runs.")
    
    if not runs:
        print("No valid runs found.")
        sys.exit(1)
    
    total_events = sum(len(r["data"]) for r in runs)
    unique_n = set(r["metadata"]["num_qubits"] for r in runs)
    unique_mappings = set(r["metadata"]["qubit_mapping"] for r in runs)
    
    print(f"\nTotal telemetry events: {total_events:,}")
    print(f"System sizes: {sorted(unique_n)}")
    print(f"Mappings: {sorted(unique_mappings)}")
    
    # Entropy analysis.
    print("\n=== Entropy Analysis ===")
    entropy_results = test_invariant_hypothesis(runs)
    print_summary(entropy_results)
    
    # Cache analysis.
    print("\n=== Cache Analysis ===")
    cache_results = test_cache_hypothesis(runs)
    print_cache_summary(cache_results)
    
    if args.json:
        with open(args.json, "w") as f:
            json.dump({
                "entropy": entropy_results,
                "cache": cache_results,
            }, f, indent=2, default=str)
        print(f"\nAnalysis results exported to {args.json}")
    
    if args.plot:
        plot_entropy_curves(entropy_results)
        plot_cache_curves(cache_results)
        plot_execution_time(runs)
    elif args.output:
        path = Path(args.output)
        path.mkdir(parents=True, exist_ok=True)
        plot_entropy_curves(entropy_results, str(path / "entropy.png"))
        plot_cache_curves(cache_results, str(path / "cache.png"))
        plot_alpha_summary(entropy_results, str(path / "alpha.png"))
        plot_execution_time(runs, str(path / "timing.png"))


if __name__ == "__main__":
    main()
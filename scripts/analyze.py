# Data loading, hypothesis testing, reporting.
#
# Usage:
#   python scripts/analyze.py --statevector results/statevector/ --mps results/mps/
#   python scripts/analyze.py --statevector results/statevector/ --json results.json
#   python scripts/analyze.py results/legacy/   (legacy single-directory statevector)

import argparse
import json
import sys
from pathlib import Path

import numpy as np


# ============================================================================
# Data Loading
# ============================================================================

def parse_telemetry_csv(filepath: str) -> tuple[dict, np.ndarray]:
    """Parse a qert statevector telemetry CSV file."""
    with open(filepath, "r") as f:
        first_line = f.readline().strip()
        if not first_line.startswith("# "):
            raise ValueError(f"Missing metadata comment in {filepath}")
        metadata = json.loads(first_line[2:])

        f.readline()  # Skip header.

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

    return metadata, np.array(rows, dtype=dtype)


def load_statevector_results(results_dir: str) -> list[dict]:
    """Load all statevector telemetry CSVs from a directory."""
    results_path = Path(results_dir)
    csv_files = sorted(results_path.glob("*.csv"))
    if not csv_files:
        raise FileNotFoundError(f"No CSV files in {results_dir}")

    runs = []
    for csv_file in csv_files:
        try:
            metadata, data = parse_telemetry_csv(str(csv_file))
            runs.append({"metadata": metadata, "data": data, "filename": csv_file.name})
        except Exception as e:
            print(f"Warning: skipping {csv_file.name}: {e}", file=sys.stderr)
    return runs


def load_mps_results(results_dir: str) -> list[dict]:
    """Load MPS simulation CSV files."""
    results_path = Path(results_dir)
    csv_files = sorted(results_path.glob("*.csv"))
    if not csv_files:
        raise FileNotFoundError(f"No CSV files in {results_dir}")

    runs = []
    for csv_file in csv_files:
        with open(csv_file) as f:
            first_line = f.readline().strip()
            if not first_line.startswith("# "):
                continue
            metadata = json.loads(first_line[2:])
            f.readline()  # Skip header.

            depths, entropy, chi_max, avg_chi = [], [], [], []
            for line in f:
                line = line.strip()
                if not line:
                    continue
                parts = line.split(",")
                depths.append(int(parts[0]))
                entropy.append(float(parts[1]))
                chi_max.append(int(parts[2]))
                avg_chi.append(float(parts[3]))

            runs.append({
                "metadata": metadata,
                "data": {"depths": depths, "entropy": entropy,
                         "chi_max": chi_max, "avg_chi": avg_chi},
                "filename": csv_file.name,
            })
    return runs


# ============================================================================
# Data Extraction
# ============================================================================

def extract_entropy_by_depth(data: np.ndarray) -> dict[int, float]:
    """Extract half-chain entropy at each sampled depth."""
    result = {}
    for row in data:
        depth = int(row["depth"])
        entropy = row["half_chain_entropy"]
        if not np.isnan(entropy):
            result[depth] = float(entropy)
    return result


def extract_cache_by_depth(data: np.ndarray) -> dict[int, list[float]]:
    """Extract per-gate L3 cache misses at each depth."""
    result = {}
    for row in data:
        depth = int(row["depth"])
        if depth not in result:
            result[depth] = []
        result[depth].append(row["l3_misses_delta"])
    return result


# ============================================================================
# Saturation Analysis
# ============================================================================

def fit_saturation_curve(depths: np.ndarray, values: np.ndarray) -> dict:
    """Fit a piecewise linear model (growth + plateau)."""
    n = len(depths)
    if n < 4:
        return {"d50": np.nan, "d90": np.nan, "inflection_depth": np.nan,
                "plateau_value": np.nan, "r_squared": np.nan,
                "fit_quality": "insufficient_data"}

    best_r2, best_split = -1.0, None
    for split_idx in range(2, n - 1):
        x1, y1 = depths[:split_idx], values[:split_idx]
        x2, y2 = depths[split_idx:], values[split_idx:]
        if len(x1) < 2 or len(x2) < 2:
            continue

        coeffs1 = np.polyfit(x1, y1, 1)
        y1_pred = np.polyval(coeffs1, x1)
        y2_pred = np.full_like(y2, np.mean(y2))

        ss_res = np.sum((y1 - y1_pred) ** 2) + np.sum((y2 - y2_pred) ** 2)
        ss_tot = np.sum((values - np.mean(values)) ** 2)
        if ss_tot < 1e-15:
            continue

        r2 = 1.0 - ss_res / ss_tot
        if r2 > best_r2:
            best_r2, best_split = r2, split_idx

    if best_split is None:
        return {"d50": np.nan, "d90": np.nan, "inflection_depth": np.nan,
                "plateau_value": np.nan, "r_squared": np.nan,
                "fit_quality": "fit_failed"}

    plateau_mean = np.mean(values[best_split:])
    d50 = _interpolate_depth(depths, values, plateau_mean * 0.5)
    d90 = _interpolate_depth(depths, values, plateau_mean * 0.9)

    return {
        "d50": d50, "d90": d90,
        "inflection_depth": float(depths[best_split]),
        "plateau_value": float(plateau_mean),
        "r_squared": float(best_r2),
        "fit_quality": "good" if best_r2 > 0.8 else "poor",
    }


def _interpolate_depth(depths: np.ndarray, values: np.ndarray, target: float) -> float:
    """Find depth where values crosses target by linear interpolation."""
    for i in range(len(values)-1):
        v1, v2 = values[i], values[i+1]

        if (v1 <= target <= v2) or (v2 <= target <= v1):

            if abs(v2 - v1) < 1e-12:
                return float(depths[i])

            frac = (target - v1) / (v2 - v1)
            return float(depths[i] + frac * (depths[i+1] - depths[i]))

    return np.nan


# ============================================================================
# Hypothesis Testing
# ============================================================================

def _group_by_n_mapping(runs: list[dict]) -> dict:
    """Group runs by (N, mapping) key."""
    conditions = {}
    for run in runs:
        meta = run["metadata"]
        key = (meta["num_qubits"], meta["qubit_mapping"])
        conditions.setdefault(key, []).append(run)
    return conditions


def test_entropy_hypothesis(runs: list[dict]) -> list[dict]:
    """Fit saturation model to entropy data for each (N, mapping)."""
    results = []
    for (n, mapping), cond_runs in _group_by_n_mapping(runs).items():
        depth_entropy = {}
        for run in cond_runs:
            for depth, ent in extract_entropy_by_depth(run["data"]).items():
                depth_entropy.setdefault(depth, []).append(ent)

        depths = np.array(sorted(depth_entropy.keys()))
        means = np.array([np.mean(depth_entropy[d]) for d in depths])
        stds = np.array([np.std(depth_entropy[d]) for d in depths])

        fit = fit_saturation_curve(depths, means)
        d_page = n / 2.0

        results.append({
            "num_qubits": n, "mapping": mapping,
            "num_seeds": len(cond_runs), "d_page": d_page,
            "d50": fit["d50"], "d90": fit["d90"],
            "inflection_depth": fit["inflection_depth"],
            "plateau_value": fit["plateau_value"],
            "r_squared": float(fit["r_squared"]),
            "fit_quality": fit["fit_quality"],
            "alpha_d90": fit["d90"] / d_page if d_page > 0 else np.nan,
            "mean_entropy_curve": [float(v) for v in means],
            "std_entropy_curve": [float(v) for v in stds],
            "depths": [int(d) for d in depths],
        })
    return results


def test_cache_hypothesis(runs: list[dict]) -> list[dict]:
    """Fit saturation model to L3 cache miss data for each (N, mapping)."""
    results = []
    for (n, mapping), cond_runs in _group_by_n_mapping(runs).items():
        depth_l3 = {}
        for run in cond_runs:
            for depth, misses in extract_cache_by_depth(run["data"]).items():
                depth_l3.setdefault(depth, []).extend(misses)

        depths = np.array(sorted(depth_l3.keys()))
        means = np.array([np.mean(depth_l3[d]) for d in depths])
        stds = np.array([np.std(depth_l3[d]) for d in depths])

        fit = fit_saturation_curve(depths, means)
        d_page = n / 2.0

        results.append({
            "num_qubits": n, "mapping": mapping,
            "num_seeds": len(cond_runs), "d_page": d_page,
            "d90_cache": fit["d90"],
            "alpha_cache": fit["d90"] / d_page if d_page > 0 else np.nan,
            "r_squared_cache": float(fit["r_squared"]),
            "fit_quality_cache": fit["fit_quality"],
            "mean_l3_curve": [float(v) for v in means],
            "std_l3_curve": [float(v) for v in stds],
            "depths": [int(d) for d in depths],
        })
    return results


# ============================================================================
# Reporting
# ============================================================================

def print_entropy_summary(results: list[dict]) -> None:
    """Print entropy hypothesis test results."""
    print("\n" + "=" * 80)
    print("ENTROPY HYPOTHESIS TEST SUMMARY")
    print("=" * 80)
    header = (f"{'N':>4s}  {'Mapping':<16s}  {'Seeds':>5s}  "
              f"{'D_Page':>8s}  {'D_90':>8s}  {'α':>10s}  "
              f"{'R²':>6s}  {'Quality':<8s}")
    print(header)
    print("-" * 80)

    for r in sorted(results, key=lambda x: (x["num_qubits"], x["mapping"])):
        print(f"{r['num_qubits']:4d}  {r['mapping']:<16s}  "
              f"{r['num_seeds']:5d}  {r['d_page']:8.1f}  "
              f"{r['d90']:8.1f}  {r['alpha_d90']:10.3f}  "
              f"{r['r_squared']:6.3f}  {r['fit_quality']:<8s}")

    print("\n--- Invariant Test ---")
    by_n = {}
    for r in results:
        by_n.setdefault(r["num_qubits"], []).append(r["alpha_d90"])
    for n in sorted(by_n):
        alphas = [a for a in by_n[n] if not np.isnan(a)]
        if len(alphas) >= 2:
            spread = max(alphas) - min(alphas)
            status = "PASS" if spread < 0.2 else "FAIL"
            print(f"  N={n:2d}: α ∈ [{min(alphas):.3f}, {max(alphas):.3f}], "
                  f"spread={spread:.3f} ({status})")
        else:
            print(f"  N={n:2d}: insufficient mappings")


def print_cache_summary(results: list[dict]) -> None:
    """Print cache hypothesis test results."""
    print("\n" + "=" * 80)
    print("CACHE HYPOTHESIS TEST SUMMARY")
    print("=" * 80)
    header = (f"{'N':>4s}  {'Mapping':<16s}  {'Seeds':>5s}  "
              f"{'D_Page':>8s}  {'D_90':>10s}  {'α_cache':>10s}  "
              f"{'R²':>6s}  {'Quality':<8s}")
    print(header)
    print("-" * 80)

    for r in sorted(results, key=lambda x: (x["num_qubits"], x["mapping"])):
        print(f"{r['num_qubits']:4d}  {r['mapping']:<16s}  "
              f"{r['num_seeds']:5d}  {r['d_page']:8.1f}  "
              f"{r['d90_cache']:10.1f}  {r['alpha_cache']:10.3f}  "
              f"{r['r_squared_cache']:6.3f}  {r['fit_quality_cache']:<8s}")

    print("\n--- Cache Invariant Test ---")
    by_n = {}
    for r in results:
        by_n.setdefault(r["num_qubits"], []).append(r["alpha_cache"])
    for n in sorted(by_n):
        alphas = [a for a in by_n[n] if not np.isnan(a)]
        if len(alphas) >= 2:
            spread = max(alphas) - min(alphas)
            status = "PASS" if spread < 0.2 else "FAIL"
            print(f"  N={n:2d}: α_cache ∈ [{min(alphas):.3f}, {max(alphas):.3f}], "
                  f"spread={spread:.3f} ({status})")


# ============================================================================
# Main
# ============================================================================

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="qert telemetry analysis")
    parser.add_argument("--statevector", type=str, default=None,
                        help="Path to statevector telemetry CSV directory")
    parser.add_argument("--mps", type=str, default=None,
                        help="Path to MPS CSV directory")
    parser.add_argument("results_dir", type=str, nargs="?", default=None,
                        help="Legacy: single statevector directory")
    parser.add_argument("--json", type=str, default=None,
                        help="Export analysis results to JSON")
    parser.add_argument("--plot", action="store_true",
                        help="Generate paper figures")
    parser.add_argument("--output", type=str, default=None,
                        help="Output directory for figures")
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    statevector_runs, mps_runs = [], []

    if args.statevector:
        print(f"Loading statevector: {args.statevector}")
        statevector_runs = load_statevector_results(args.statevector)
        print(f"Loaded {len(statevector_runs)} runs.")
    elif args.results_dir:
        print(f"Loading (legacy): {args.results_dir}")
        statevector_runs = load_statevector_results(args.results_dir)
        print(f"Loaded {len(statevector_runs)} runs.")

    if args.mps:
        print(f"Loading MPS: {args.mps}")
        mps_runs = load_mps_results(args.mps)
        print(f"Loaded {len(mps_runs)} runs.")

    if not statevector_runs and not mps_runs:
        print("No data loaded.")
        sys.exit(1)

    entropy_results, cache_results = None, None

    if statevector_runs:
        total_events = sum(len(r["data"]) for r in statevector_runs)
        unique_n = set(r["metadata"]["num_qubits"] for r in statevector_runs)
        print(f"\nStatevector events: {total_events:,}")
        print(f"System sizes: {sorted(unique_n)}")

        entropy_results = test_entropy_hypothesis(statevector_runs)
        print_entropy_summary(entropy_results)

        cache_results = test_cache_hypothesis(statevector_runs)
        print_cache_summary(cache_results)

    if mps_runs:
        unique_n = set(r["metadata"]["num_qubits"] for r in mps_runs)
        print(f"\nMPS runs: {len(mps_runs)}")
        print(f"System sizes: {sorted(unique_n)}")

    if args.json:
        output = {}
        if entropy_results:
            output["entropy"] = entropy_results
        if cache_results:
            output["cache"] = cache_results
        with open(args.json, "w") as f:
            json.dump(output, f, indent=2, default=str)
        print(f"\nExported to {args.json}")

    if args.plot or args.output:
        from figures import generate_paper_figures
        generate_paper_figures(
            statevector_runs=statevector_runs,
            mps_runs=mps_runs,
            entropy_results=entropy_results,
            cache_results=cache_results,
            output_dir=args.output,
        )


if __name__ == "__main__":
    main()
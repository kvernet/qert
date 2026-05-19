"""
run_experiments.py — Parameter sweep orchestrator for qert.

Generates experiment configurations, invokes the qert binary for each run,
and collects output CSVs into a structured results directory.

Usage:
    python scripts/run_experiments.py --config experiments/phase1.json
    python scripts/run_experiments.py --quick-test  # Small validation sweep

Output structure:
    results/
        YYYY-MM-DD_HH-MM-SS/
            run_n12_d36_s42_lex.csv
            run_n12_d36_s42_gray.csv
            ...
            _summary.json
"""

import argparse
import json
import os
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path


# --- Default experimental configuration ---
DEFAULT_CONFIG = {
    "circuit_family": "brickwall_1d",
    "system_sizes": [12, 16, 20],
    "depths_per_n": {
        12: [12, 24, 36],
        16: [16, 32, 48],
        20: [20, 40, 60],
    },
    "mappings": ["lexicographic", "gray", "locality_aware"],
    "seeds_per_condition": 10,  # Phase 1: 10 for quick tests, 100 for real
    "qert_binary": "./build/qert",
}


def parse_args():
    parser = argparse.ArgumentParser(description="qert experiment orchestrator")
    parser.add_argument("--config", type=str, help="Path to JSON config file")
    parser.add_argument("--quick-test", action="store_true",
                        help="Run a minimal sweep (N=12, 2 seeds, 2 depths)")
    parser.add_argument("--qert", type=str, default="./build/qert",
                        help="Path to qert binary")
    return parser.parse_args()


def build_experiments(config: dict) -> list[dict]:
    """Build the list of (N, depth, seed, mapping) tuples to run."""
    experiments = []

    for n in config["system_sizes"]:
        depths = config["depths_per_n"].get(str(n), config["depths_per_n"].get(n, []))
        for depth in depths:
            for mapping in config["mappings"]:
                for seed_idx in range(config["seeds_per_condition"]):
                    seed = 42 + seed_idx * 1000 + n * 100000 + depth  # Deterministic seeds
                    experiments.append({
                        "num_qubits": n,
                        "depth": depth,
                        "seed": seed,
                        "mapping": mapping,
                        "circuit_family": config["circuit_family"],
                    })

    return experiments


def run_single(qert_binary: str, params: dict, output_path: str) -> bool:
    """Run a single experiment. Returns True on success."""
    cmd = [
        qert_binary,
        "--num-qubits", str(params["num_qubits"]),
        "--depth", str(params["depth"]),
        "--seed", str(params["seed"]),
        "--mapping", params["mapping"],
        "--circuit-family", params["circuit_family"],
        "--output", output_path,
    ]

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=3600,  # 1 hour max per run
        )

        if result.returncode != 0:
            print(f"  FAILED (exit code {result.returncode})")
            print(f"  stderr: {result.stderr.strip()}")
            return False

        # Verify output file exists and has content.
        if not os.path.exists(output_path) or os.path.getsize(output_path) < 100:
            print(f"  FAILED (empty or missing output)")
            return False

        return True

    except subprocess.TimeoutExpired:
        print(f"  TIMEOUT")
        return False
    except FileNotFoundError:
        print(f"  ERROR: qert binary not found at '{qert_binary}'")
        sys.exit(1)


def main():
    args = parse_args()

    # Load configuration.
    if args.quick_test:
        config = {
            "circuit_family": "brickwall_1d",
            "system_sizes": [12],
            "depths_per_n": {12: [12, 24]},
            "mappings": ["lexicographic"],
            "seeds_per_condition": 2,
        }
        print("Running quick test sweep...")
    elif args.config:
        with open(args.config) as f:
            config = json.load(f)
        print(f"Loaded config from {args.config}")
    else:
        config = DEFAULT_CONFIG
        print("Using default configuration.")

    qert_binary = args.qert
    if not os.path.exists(qert_binary):
        print(f"Error: qert binary not found at '{qert_binary}'")
        print("Build with: cmake -B build && cmake --build build")
        sys.exit(1)

    # Create output directory.
    timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    output_dir = Path("results") / timestamp
    output_dir.mkdir(parents=True, exist_ok=True)
    print(f"Output directory: {output_dir}")

    # Build experiment list.
    experiments = build_experiments(config)
    total = len(experiments)
    print(f"Total experiments: {total}")
    print(f"  System sizes: {config['system_sizes']}")
    print(f"  Mappings: {config['mappings']}")
    print(f"  Seeds per condition: {config['seeds_per_condition']}")
    print()

    # Run experiments.
    succeeded = 0
    failed = 0
    summary = {
        "config": config,
        "timestamp": timestamp,
        "total": total,
        "succeeded": 0,
        "failed": 0,
        "runs": [],
    }

    t_start = time.time()

    for idx, exp in enumerate(experiments):
        filename = (
            f"run_n{exp['num_qubits']:02d}"
            f"_d{exp['depth']:03d}"
            f"_s{exp['seed']:05d}"
            f"_{exp['mapping']}.csv"
        )
        output_path = output_dir / filename

        progress = f"[{idx + 1}/{total}]"
        desc = (
            f"N={exp['num_qubits']:2d} depth={exp['depth']:3d} "
            f"seed={exp['seed']:5d} mapping={exp['mapping']}"
        )

        print(f"{progress} {desc} ... ", end="", flush=True)

        success = run_single(qert_binary, exp, str(output_path))

        if success:
            succeeded += 1
            print("OK")
        else:
            failed += 1

        summary["runs"].append({
            "params": exp,
            "output": filename,
            "success": success,
        })

        # Progress estimate every 10 runs.
        if (idx + 1) % 10 == 0:
            elapsed = time.time() - t_start
            rate = (idx + 1) / elapsed
            remaining = (total - idx - 1) / rate
            print(f"  ... {idx + 1}/{total} done, "
                  f"~{remaining:.0f}s remaining\n")

    # Write summary.
    summary["succeeded"] = succeeded
    summary["failed"] = failed
    summary["elapsed_seconds"] = time.time() - t_start

    summary_path = output_dir / "_summary.json"
    with open(summary_path, "w") as f:
        json.dump(summary, f, indent=2)

    print(f"\n{'=' * 60}")
    print(f"Complete: {succeeded} succeeded, {failed} failed out of {total}")
    print(f"Elapsed: {summary['elapsed_seconds']:.0f}s")
    print(f"Results: {output_dir}")
    print(f"Summary: {summary_path}")

    if failed > 0:
        sys.exit(1)


if __name__ == "__main__":
    main()
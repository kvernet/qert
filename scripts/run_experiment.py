"""
Single-circuit experiment runner for qert.

Runs one circuit to depth 3N, sampling half-chain entropy at every even
layer during execution. Designed to be called by sweep.sh for parallel
parameter sweeps, or directly for single experiments.

Usage:
    # Single experiment:
    python scripts/run_experiments.py --num-qubits 12 --seed 42 --mapping lexicographic

    # Via sweep.sh (parallel):
    bash scripts/sweep.sh 12 lexicographic 100
"""

import argparse
import os
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(
        description="Run a single qert experiment to depth 3N"
    )
    parser.add_argument(
        "--qert", type=str, default="./build/qert",
        help="Path to qert binary (default: ./build/qert)"
    )
    parser.add_argument(
        "--circuit-family", type=str, default="brickwall_1d",
        help="Circuit family (default: brickwall_1d)"
    )
    parser.add_argument(
        "--num-qubits", type=int, required=True,
        help="Number of qubits"
    )
    parser.add_argument(
        "--seed", type=int, required=True,
        help="Random seed (non-zero)"
    )
    parser.add_argument(
        "--mapping", type=str, required=True,
        choices=["lexicographic", "gray", "locality_aware"],
        help="Qubit mapping strategy"
    )
    parser.add_argument(
        "--output-dir", type=str, default=None,
        help="Output directory (default: results/<timestamp>/)"
    )
    parser.add_argument(
        "--timeout", type=int, default=7200,
        help="Timeout in seconds (default: 7200 = 2 hours)"
    )
    parser.add_argument(
        "--quiet", action="store_true",
        help="Suppress output on success"
    )
    return parser.parse_args()


def main():
    args = parse_args()

    qert_binary = args.qert
    if not os.path.exists(qert_binary):
        print(f"Error: qert binary not found at '{qert_binary}'", file=sys.stderr)
        print("Build with: cmake -B build && cmake --build build", file=sys.stderr)
        sys.exit(1)

    # Circuit depth is always 3N.
    depth = 3 * args.num_qubits

    # Output directory.
    if args.output_dir:
        output_dir = Path(args.output_dir)
    else:
        timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        output_dir = Path("results") / timestamp
    output_dir.mkdir(parents=True, exist_ok=True)

    output_path = output_dir / (
        f"run_n{args.num_qubits:02d}"
        f"_d{depth:03d}"
        f"_s{args.seed:05d}"
        f"_{args.mapping}.csv"
    )

    cmd = [
        qert_binary,
        "--num-qubits", str(args.num_qubits),
        "--depth", str(depth),
        "--seed", str(args.seed),
        "--mapping", args.mapping,
        "--circuit-family", args.circuit_family,
        "--output", str(output_path),
    ]

    t_start = time.time()

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=args.timeout,
        )

        elapsed = time.time() - t_start

        if result.returncode != 0:
            print(
                f"FAILED (exit {result.returncode}) "
                f"N={args.num_qubits} seed={args.seed} "
                f"mapping={args.mapping} ({elapsed:.0f}s)",
                file=sys.stderr,
            )
            if result.stderr:
                print(f"  {result.stderr.strip()}", file=sys.stderr)
            sys.exit(1)

        if not os.path.exists(output_path) or os.path.getsize(output_path) < 100:
            print(
                f"FAILED (missing/empty output) "
                f"N={args.num_qubits} seed={args.seed} "
                f"mapping={args.mapping} ({elapsed:.0f}s)",
                file=sys.stderr,
            )
            sys.exit(1)

        if not args.quiet:
            print(
                f"OK ({elapsed:.0f}s) "
                f"N={args.num_qubits} depth={depth} "
                f"seed={args.seed} mapping={args.mapping} "
                f"-> {output_path.name}"
            )

    except subprocess.TimeoutExpired:
        elapsed = time.time() - t_start
        print(
            f"TIMEOUT (>{args.timeout}s) "
            f"N={args.num_qubits} seed={args.seed} "
            f"mapping={args.mapping} ({elapsed:.0f}s)",
            file=sys.stderr,
        )
        sys.exit(1)


if __name__ == "__main__":
    main()
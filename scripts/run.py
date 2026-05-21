"""
Run a single qert experiment with full CLI parameterization.

Designed for parallel orchestration via GNU parallel, xargs, or shell loops.
Each invocation runs exactly one circuit and produces one telemetry CSV.

Usage:
    python scripts/run.py \
        --num-qubits 16 --depth 48 --seed 42 \
        --mapping lexicographic --output results/run.csv

    # Parallel sweep with GNU parallel:
    parallel -j 8 python scripts/run.py \
        --num-qubits 16 --depth 48 --seed {} --output results/run_s{}.csv \
        ::: $(seq 42 141)
"""

import argparse
import os
import subprocess
import sys
import time


def parse_args():
    parser = argparse.ArgumentParser(
        description="Run a single qert experiment"
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
        "--depth", type=int, required=True,
        help="Circuit depth (number of layers)"
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
        "--output", type=str, required=True,
        help="Output CSV file path"
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


def run_single(
    qert_binary: str,
    circuit_family: str,
    num_qubits: int,
    depth: int,
    seed: int,
    mapping: str,
    output_path: str,
    timeout: int,
) -> bool:
    """Run a single experiment. Returns True on success."""
    # Ensure output directory exists.
    output_dir = os.path.dirname(output_path)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)

    cmd = [
        qert_binary,
        "--num-qubits", str(num_qubits),
        "--depth", str(depth),
        "--seed", str(seed),
        "--mapping", mapping,
        "--circuit-family", circuit_family,
        "--output", output_path,
    ]

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
        )

        if result.returncode != 0:
            print(f"FAILED (exit code {result.returncode})", file=sys.stderr)
            if result.stderr:
                print(f"  stderr: {result.stderr.strip()}", file=sys.stderr)
            return False

        # Verify output file exists and has content.
        if not os.path.exists(output_path):
            print(f"FAILED (output file not created)", file=sys.stderr)
            return False

        file_size = os.path.getsize(output_path)
        if file_size < 100:
            print(f"FAILED (output file too small: {file_size} bytes)", file=sys.stderr)
            return False

        return True

    except subprocess.TimeoutExpired:
        print(f"TIMEOUT (>{timeout}s)", file=sys.stderr)
        return False
    except FileNotFoundError:
        print(f"ERROR: qert binary not found at '{qert_binary}'", file=sys.stderr)
        print("Build with: cmake -B build && cmake --build build", file=sys.stderr)
        sys.exit(1)


def main():
    args = parse_args()

    if not os.path.exists(args.qert):
        print(f"Error: qert binary not found at '{args.qert}'", file=sys.stderr)
        print("Build with: cmake -B build && cmake --build build", file=sys.stderr)
        sys.exit(1)

    t_start = time.time()

    success = run_single(
        qert_binary=args.qert,
        circuit_family=args.circuit_family,
        num_qubits=args.num_qubits,
        depth=args.depth,
        seed=args.seed,
        mapping=args.mapping,
        output_path=args.output,
        timeout=args.timeout,
    )

    elapsed = time.time() - t_start

    if success:
        if not args.quiet:
            print(
                f"OK ({elapsed:.1f}s) "
                f"N={args.num_qubits} depth={args.depth} "
                f"seed={args.seed} mapping={args.mapping} "
                f"-> {args.output}"
            )
        sys.exit(0)
    else:
        sys.exit(1)


if __name__ == "__main__":
    main()
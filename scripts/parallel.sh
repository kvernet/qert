#!/usr/bin/env bash
#
# Parallel parameter sweep for qert experiments.
# Runs a single circuit to depth 3N per (N, mapping, seed), sampling
# half-chain entropy at every even layer during execution.
#
# Usage:
#   bash scripts/parallel.sh <N> <mapping> <num_seeds> [base_seed] [parallel_jobs]
#
# Examples:
#   bash scripts/parallel.sh 16 lexicographic 100           # 100 seeds, depth=48
#   bash scripts/parallel.sh 20 gray 50 42 4                # 50 seeds, base_seed=42, 4 parallel
#
# Output files:
#   results/<timestamp>/run_n<N>_d<3N>_s<seed>_<mapping>.csv

set -euo pipefail

N=${1:?Usage: $0 <N> <mapping> <num_seeds> [base_seed] [parallel_jobs]}
MAPPING=${2:?}
NUM_SEEDS=${3:?}
BASE_SEED=${4:-42}
PARALLEL_JOBS=${5:-$(( $(nproc) - 2 ))}

DEPTH=$(( 3 * N ))
QERT_BIN="${QERT_BIN:-./build/qert}"

if [ "$PARALLEL_JOBS" -lt 1 ]; then
    PARALLEL_JOBS=1
fi

TIMESTAMP=$(date +%Y-%m-%d_%H-%M-%S)
OUTPUT_DIR="results/${TIMESTAMP}"
mkdir -p "$OUTPUT_DIR"

echo "============================================"
echo "qert parameter sweep"
echo "============================================"
echo "N              = $N"
echo "Depth          = $DEPTH (3N)"
echo "Mapping        = $MAPPING"
echo "Seeds          = $NUM_SEEDS (base=$BASE_SEED)"
echo "Parallel jobs  = $PARALLEL_JOBS"
echo "Binary         = $QERT_BIN"
echo "Output dir     = $OUTPUT_DIR"
echo "============================================"
echo ""

for i in $(seq 0 $((NUM_SEEDS - 1))); do
    SEED=$((BASE_SEED + i * 1000))
    OUTPUT="${OUTPUT_DIR}/run_n${N}_d${DEPTH}_s${SEED}_${MAPPING}.csv"
    echo "$QERT_BIN --num-qubits $N --depth $DEPTH --seed $SEED --mapping $MAPPING --output $OUTPUT"
done | parallel -j "$PARALLEL_JOBS" --bar

SUCCEEDED=$(ls "$OUTPUT_DIR"/*.csv 2>/dev/null | wc -l)
FAILED=$((NUM_SEEDS - SUCCEEDED))

echo ""
echo "============================================"
echo "Complete: $SUCCEEDED succeeded, $FAILED failed (out of $NUM_SEEDS)"
echo "Results: $OUTPUT_DIR"
echo "============================================"

if [ "$FAILED" -gt 0 ]; then
    echo "Warning: $FAILED runs failed. Check individual outputs for errors."
fi
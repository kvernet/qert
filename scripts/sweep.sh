#!/usr/bin/env bash

#
# Parallel parameter sweep for qert experiments.
#
# Usage:
#   bash scripts/sweep.sh <N> <depth> <mapping> <num_seeds> [base_seed] [parallel_jobs]
#
# Examples:
#   bash scripts/sweep.sh 12 36 lexicographic 100        # 100 seeds, all cores-2
#   bash scripts/sweep.sh 16 48 gray 50 42 4              # 50 seeds, base_seed=42, 4 parallel
#   bash scripts/sweep.sh 20 60 locality_aware 10 12345 2  # N=20, limited to 2 parallel
#
# Output files are written to results/ with names:
#   run_n<N>_d<depth>_s<seed>_<mapping>.csv

set -euo pipefail

N=${1:?Usage: $0 <N> <depth> <mapping> <num_seeds> [base_seed] [parallel_jobs]}
DEPTH=${2:?}
MAPPING=${3:?}
NUM_SEEDS=${4:?}
BASE_SEED=${5:-42}
PARALLEL_JOBS=${6:-$(( $(nproc) - 2 ))}

# Ensure parallel jobs is at least 1.
if [ "$PARALLEL_JOBS" -lt 1 ]; then
    PARALLEL_JOBS=1
fi

# Create output directory.
TIMESTAMP=$(date +%Y-%m-%d_%H-%M-%S)
OUTPUT_DIR="results/${TIMESTAMP}"
mkdir -p "$OUTPUT_DIR"

echo "============================================"
echo "qert parameter sweep"
echo "============================================"
echo "N              = $N"
echo "Depth          = $DEPTH"
echo "Mapping        = $MAPPING"
echo "Seeds          = $NUM_SEEDS (base=$BASE_SEED)"
echo "Parallel jobs  = $PARALLEL_JOBS"
echo "Output dir     = $OUTPUT_DIR"
echo "============================================"
echo ""

# Generate command list and run in parallel.
# Each seed is offset by 1000 to avoid collisions across different (N,depth) combinations.
for i in $(seq 0 $((NUM_SEEDS - 1))); do
    SEED=$((BASE_SEED + i * 1000))
    OUTPUT="${OUTPUT_DIR}/run_n${N}_d${DEPTH}_s${SEED}_${MAPPING}.csv"
    echo "python scripts/run.py --num-qubits $N --depth $DEPTH --seed $SEED --mapping $MAPPING --output $OUTPUT --quiet"
done | parallel -j "$PARALLEL_JOBS" --bar

# Count results.
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
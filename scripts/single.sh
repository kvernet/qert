#!/usr/bin/env bash
# Usage:
#   bash scripts/single.sh <N> <mapping> <num_seeds> [base_seed]
#
# Example:
#   bash scripts/single.sh 16 lexicographic 10 42

set -euo pipefail

N=${1:?}
MAPPING=${2:?}
NUM_SEEDS=${3:?}
BASE_SEED=${4:-42}
DEPTH=$(( 3 * N ))
QERT_BIN="${QERT_BIN:-./build/qert}"

TIMESTAMP=$(date +%Y-%m-%d_%H-%M-%S)
OUTPUT_DIR="results/single_${TIMESTAMP}"
mkdir -p "$OUTPUT_DIR"

echo "=== Single-process sweep ==="
echo "N=$N  depth=$DEPTH  mapping=$MAPPING  seeds=$NUM_SEEDS"
echo "Output: $OUTPUT_DIR"
echo ""

for i in $(seq 0 $((NUM_SEEDS - 1))); do
    SEED=$((BASE_SEED + i * 10))
    OUTPUT="${OUTPUT_DIR}/run_n${N}_d${DEPTH}_s${SEED}_${MAPPING}.csv"
    
    START=$(date +%s)
    echo -n "[$((i+1))/$NUM_SEEDS] seed=$SEED ... "
    
    $QERT_BIN \
        --num-qubits "$N" \
        --depth "$DEPTH" \
        --seed "$SEED" \
        --mapping "$MAPPING" \
        --output "$OUTPUT"
    
    END=$(date +%s)
    echo "OK ($((END-START))s)"
done

SUCCEEDED=$(ls "$OUTPUT_DIR"/*.csv 2>/dev/null | wc -l) || true
echo ""
echo "Done: $SUCCEEDED/$NUM_SEEDS succeeded"
echo "Results: $OUTPUT_DIR"
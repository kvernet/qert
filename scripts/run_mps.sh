#!/usr/bin/env bash
set -euo pipefail

run=${1:-1}

MAPPING=lexicographic
NUM_SEEDS=1000

QERT_BIN="${QERT_BIN:-./build/qert_mps}"

TIMESTAMP=$(date +%Y-%m-%d_%H-%M-%S)
OUTPUT_DIR="results/mps_${TIMESTAMP}"
mkdir -p "$OUTPUT_DIR"

compute_base() {
    N=$1
    MAPPING_OFFSET=$2
    echo $((N * 10000000 + run * 1000000 + MAPPING_OFFSET * 100000))
}

for N in 8 10 12 16 18; do
    DEPTH=$((3*N))
    BASE_SEED=$(compute_base $N 0)
    for i in $(seq 0 $((NUM_SEEDS - 1))); do
        SEED=$((BASE_SEED + i * 10))
        OUTPUT="${OUTPUT_DIR}/run_n${N}_d${DEPTH}_s${SEED}_${MAPPING}.csv"
        
        START=$(date +%s)
        echo -n "[$((i+1))/$NUM_SEEDS] seed=$SEED ... "

        $QERT_BIN \
            --num-qubits $N \
            --depth $DEPTH \
            --mapping $MAPPING \
            --seed $SEED \
            --chi-max 64 \
            --output $OUTPUT
    done
done
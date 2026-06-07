#!/usr/bin/env bash
set -euo pipefail

BASE_SEED=${1:-42}

for N in 16 18 20 22; do
    BASE_SEED=$((BASE_SEED + i * 1000))
    for mapping in lexicographic gray locality_aware; do
        bash scripts/parallel.sh $N $mapping 100 $BASE_SEED
        BASE_SEED=$((BASE_SEED + 10000))
    done
done
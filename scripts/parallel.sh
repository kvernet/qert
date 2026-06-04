#!/usr/bin/env bash

BASE_SEED=42

for N in 4 6 8 10 12 14 16 18; do
    for MAPPING in lexicographic gray locality_aware; do
        bash scripts/sweep.sh $N $MAPPING 100 $BASE_SEED 16
        BASE_SEED=$((BASE_SEED + 100000))
    done
done
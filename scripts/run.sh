#!/usr/bin/env bash

base_seed=${1:-42}

for N in 4 6 8 10 12 14 16 18; do
    for i in $(seq 1 3); do
        for mapping in lexicographic gray locality_aware; do
            seed=$(( base_seed + 1000 * N + i ))
            ./scripts/sweep.sh $N $(( N * i )) $mapping 100 $seed 16
        done
    done
done
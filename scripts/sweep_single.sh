#!/usr/bin/env bash
set -euo pipefail

run=${1:-1}

compute_base() {
    N=$1
    MAPPING_OFFSET=$2
    echo $((N * 10000000 + run * 1000000 + MAPPING_OFFSET * 100000))
}

sweep_single() {
    N=$1
    NUM_SEEDS=$2
    for mapping in lexicographic gray locality_aware; do
        case $mapping in
            lexicographic) base=$(compute_base $N 0) ;;
            gray)          base=$(compute_base $N 1) ;;
            locality_aware) base=$(compute_base $N 2) ;;
        esac
        bash single.sh $N $mapping $NUM_SEEDS $base
    done
}

for N in $(seq 8 2 18); do
    sweep_single $N 500
done
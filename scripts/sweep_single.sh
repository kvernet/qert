#!/usr/bin/env bash
set -euo pipefail

compute_base() {
    N=$1
    MAPPING_OFFSET=$2
    echo $((N * 1000000 + MAPPING_OFFSET * 100000 + 42))
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
        bash scripts/single.sh $N $mapping $NUM_SEEDS $base
    done
}

# N=16: 100 seeds per mapping (~7 min)
sweep_single 16 100

# N=18: 100 seeds per mapping (~1.5 hours)
sweep_single 18 100

# N=20: 100 seeds per mapping (~12 hours)
sweep_single 20 100

# N=22: 100 seeds per mapping (~120 hours)
#sweep_single 22 100
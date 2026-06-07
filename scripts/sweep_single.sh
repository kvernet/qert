#!/usr/bin/env bash
set -euo pipefail

# Each (N, mapping) gets a unique base seed.
# Formula: base = 100000 * N + offset
# N=16: base 1600042, 1601042, 1602042
# N=18: base 1800042, 1801042, 1802042
# etc.

# N=16: 100 seeds per mapping (~7 min)
for mapping in lexicographic gray locality_aware; do
    case $mapping in
        lexicographic) base=1600042 ;;
        gray)          base=1601042 ;;
        locality_aware) base=1602042 ;;
    esac
    bash scripts/single.sh 16 $mapping 100 $base
done

# N=18: 50 seeds per mapping (~40 min)
for mapping in lexicographic gray locality_aware; do
    case $mapping in
        lexicographic) base=1800042 ;;
        gray)          base=1801042 ;;
        locality_aware) base=1802042 ;;
    esac
    bash scripts/single.sh 18 $mapping 50 $base
done

# N=20: 30 seeds per mapping (~3.6 hours)
for mapping in lexicographic gray locality_aware; do
    case $mapping in
        lexicographic) base=2000042 ;;
        gray)          base=2001042 ;;
        locality_aware) base=2002042 ;;
    esac
    bash scripts/single.sh 20 $mapping 30 $base
done

# N=22: 10 seeds per mapping (~12 hours)
for mapping in lexicographic gray locality_aware; do
    case $mapping in
        lexicographic) base=2200042 ;;
        gray)          base=2201042 ;;
        locality_aware) base=2202042 ;;
    esac
    bash scripts/single.sh 22 $mapping 10 $base
done
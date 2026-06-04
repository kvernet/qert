BASE_SEED=42

TIMESTAMP=$(date +%Y-%m-%d_%H-%M-%S)
OUTPUT_DIR="results/${TIMESTAMP}"
mkdir -p "$OUTPUT_DIR"

for N in 4 6 8 10 12 14 16 18; do
    DEPTH=$(( 3 * N ))
    for MAPPING in lexicographic gray locality_aware; do
        for i in $(seq 0 99); do
			SEED=$((BASE_SEED + i * 1000))
			OUTPUT="${OUTPUT_DIR}/run_n${N}_d${DEPTH}_s${SEED}_${MAPPING}.csv"
			python scripts/run_experiment.py --num-qubits $N --seed $SEED --mapping $MAPPING --output-dir $OUTPUT_DIR
		done
        BASE_SEED=$((BASE_SEED + 100000))
    done
done
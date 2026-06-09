# qert — Quantum Execution Runtime Telemetry

An instrumented statevector simulator designed as a scientific observatory
for studying the relationship between quantum entanglement structure and
classical execution behavior.

## Overview

qert is not a production simulator. It does not optimize for speed.
It optimizes for **observability**: every gate application produces a
telemetry event recording execution time, entanglement entropy, and
hardware performance counters (L3 cache misses, TLB misses).

The central question: **does entanglement structure leave measurable
signatures in classical execution behavior?**

Answer (at N ≤ 20, statevector in cache): **no**. Cache behavior is
dominated by circuit layer structure, not by entanglement dynamics.

## Architecture

```
qert/
├── app
│   └── main.cpp                            # CLI experiment runner
├── CMakeLists.txt
├── core
│   ├── include
│   │   └── qert                            # Public headers
│   │       ├── build_info.hpp.in
│   │       ├── circuit.hpp
│   │       ├── common.hpp
│   │       ├── entropy.hpp
│   │       ├── gates.hpp
│   │       ├── hardware.hpp
│   │       ├── seed.hpp
│   │       ├── statevector.hpp
│   │       └── telemetry.hpp
│   └── src                                 # Implementation
│       ├── circuit.cpp
│       ├── entropy.cpp
│       ├── gates.cpp
│       ├── hardware.cpp
│       ├── seed.cpp
│       ├── statevector.cpp
│       └── telemetry.cpp
├── docs
│   └── HYPOTHESIS.md                       # Locked hypothesis specification
├── LICENSE.txt
├── Makefile
├── README.md
├── scripts                                 # Experiment orchestration and analysis
├── telemetry_schema/                       # CSV format specification
└── tests/                                  # Catch2 test suite (44 tests, 522 assertions)
```

## Quick Start

### Prerequisites

```bash
make dev-install
```

### Build

```bash
# Release build with tests
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=ON
cmake --build build

# With PAPI hardware counters (requires PAPI installed)
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=ON \
    -DENABLE_PAPI=ON \
    -DPAPI_ROOT=papi-install-root-dir
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure
```

### Run an experiment

```bash
./build/qert \
    --num-qubits 16 \
    --depth 48 \
    --seed 42 \
    --mapping lexicographic \
    --output run.csv

# Check the telemetry
head -5 run.csv
```

### Run a sweep

```bash
# Single-process sweep
bash scripts/single.sh 20 lexicographic 100

# Parallel sweep
bash scripts/parallel.sh 16 lexicographic 100
```

### Analyze results

```bash
python scripts/analyze.py results/2026-06-09_12-00-00/ --output figures/
```

## Key Findings

### Phase 1: Entropy characterization (N=4-18, 2,400 runs)
- Half-chain entropy saturates at ᾱ ≈ 1.60 × Page time
- No convergence toward α = 1.0
- Parity-sensitive oscillation (even vs odd N/2): Δα ≈ 0.08
- All saturation fits: R² > 0.97

### Phase 2: Cache-entanglement hypothesis (N=16-20, 900 runs)
- L3 cache miss patterns show **no correlation** with entanglement growth
- Saturation model fails for cache data: R² < 0.2
- Cache behavior dominated by circuit layer structure
- Hypothesis **falsified** in the cache-contained regime

## Dependencies

| Dependency | Version | Required | Notes |
|-----------|---------|----------|-------|
| C++17 | — | Yes | Standard library |
| CMake | ≥ 3.16 | Yes | Build system |
| Eigen | 3.4.1 | Yes (Phase 2) | Fetched automatically via CMake |
| PAPI | ≥ 7.3 | Optional | Hardware counters. `ENABLE_PAPI=OFF` for stubs |
| Catch2 | 3.15.0 | Tests | Downloaded automatically |
| Python | ≥ 3.10 | Analysis | numpy, matplotlib |

## Supported Platforms

| Platform | Compiler | Status |
|----------|----------|--------|
| Ubuntu 24.04 | GCC 13, Clang 18 | ✅ Full support |
| macOS | Apple Clang | ✅ Builds and tests pass |
| Windows | MSVC | ✅ Builds and tests pass |
| ARM (Apple Silicon) | Clang | ⬜ Not yet tested |

## Reproducibility

Every telemetry CSV is self-describing: the first line contains a JSON
metadata header with:

- Circuit identity (family, N, depth, mapping, RNG seed)
- Build identity (git commit, compiler, flags)
- Platform identity (CPU model, architecture)
- Three deterministic FNV-1a 64-bit hashes for experiment fingerprinting

Two runs with the same `physics_hash` test the same scientific condition
regardless of platform or build environment.

## Paper

[QERT paper](https://kvernet.com/qert)

## Citation

If you use qert in your research, please cite:

```bibtex
@misc{qert2026,
  title={{qert}: Quantum Execution Runtime Telemetry},
  author={Kinson Vernet},
  year={2026},
  howpublished={\url{https://github.com/kvernet/qert}}
}
```

## License

MIT License. See `LICENSE` file.

## Related Repositories

- [qert-data](https://github.com/kvernet/qert-data) — Dataset
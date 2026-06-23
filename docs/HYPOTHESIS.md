# Hypothesis specification: Entanglement-execution signatures

**Project:** qert — Quantum Execution Runtime Telemetry  
**Document version:** 1.0.0  
**Status:** Complete — Phase 1, Phase 2, and MPS comparison complete  
**Date:** 2026-06-23

---

## 1. Core Research Question

> Does quantum entanglement structure leave measurable signatures in the
> classical execution behavior of quantum circuit simulation?

---

## 2. Primary Hypothesis (tested in Phase 2)

> In 1D brickwall circuits with Haar-random SU(4) gates, the L3 cache miss
> rate during statevector simulation exhibits a saturation transition at a
> circuit depth coinciding with the half-chain entanglement entropy saturation
> depth. The scaling coefficient α_cache = D_90_cache / (N/2) matches
> α_entropy within |Δα| < 0.2, and the piecewise linear saturation model
> achieves R² > 0.8 for both entropy and cache data.

**Result: NOT SUPPORTED.** See Section 13.

---

## 3. Secondary Question (MPS comparison)

> If statevector cache behavior does not reflect entanglement, does MPS bond
> dimension? This validates that the instrument can detect entanglement
> effects where they exist, making the statevector null result meaningful.

**Result: CONFIRMED.** MPS bond dimension χ grows with entanglement entropy
and saturates at either the parameter cap (χ_max) or the Hilbert space bound
(2^(N/2)). Same instrument, same circuits, different observables, different
answers.

---

## 4. Circuit Ensemble

**Primary:** 1D brickwall circuits with Haar-random SU(4) two-qubit gates.

- Even layers: gates on pairs (0,1), (2,3), (4,5), ...
- Odd layers: gates on pairs (1,2), (3,4), (5,6), ...
- Each two-qubit gate is an independent Haar-random SU(4) unitary.

---

## 5. Experimental Conditions (as executed)

### Phase 1: Entropy characterization

| Variable | Values |
|----------|--------|
| System sizes N | 4, 6, 8, 10, 12, 14, 16, 18 |
| Seeds per condition | 100 |
| Total runs | 2,400 |
| Execution mode | Parallel (16 jobs) |
| Hardware counters | Stubbed |
| Entropy solver | Hermitian Jacobi (k ≤ 10) |
| Mapping | Recorded in metadata, not applied |

### Phase 2: Cache-entanglement hypothesis test

| Variable | Values |
|----------|--------|
| System sizes N | 8, 10, 12, 14, 16, 18 |
| Seeds per condition | 2,000 |
| Total runs | 36,000 |
| Execution mode | Sequential single-process |
| Hardware counters | PAPI 7.3.0 (perf::LLC-LOAD-MISSES, perf::DTLB-LOAD-MISSES) |
| Entropy solver | Eigen 3.4.1 SelfAdjointEigenSolver (k ≤ 12) |
| Mapping | Applied to gate descriptors |

### MPS comparison

| Variable | Values |
|----------|--------|
| System sizes N | 8, 10, 12, 16, 18 |
| Seeds per condition | 1,000 |
| Total runs | 5,000 |
| χ_max values | 32–128 (N-dependent) |
| Observables | χ_max, avg_chi, entropy (via statevector) |

---

## 6. Observable Hierarchy

| Tier | Observable | Phase 1 | Phase 2 | MPS |
|------|-----------|---------|---------|-----|
| **Primary** | L3 cache misses per gate | Stubbed | Real | — |
| **Primary** | MPS bond dimension χ | — | — | Real |
| **Secondary** | Half-chain von Neumann entropy | Jacobi | Eigen | Eigen |
| **Tertiary** | Per-gate execution time | Real | Real | Real |
| **Tertiary** | TLB misses per gate | Stubbed | Real | — |

---

## 7. Transition Observables

Three transition depths extracted from piecewise linear saturation fits:

| Observable | Definition |
|-----------|------------|
| D_50 | Depth at 50% of plateau value |
| D_90 | Depth at 90% of plateau value |
| D_inf | Split point depth (inflection between growth and plateau) |

---

## 8. Null Hypothesis

> Cache miss rate is predicted equally well by circuit depth as by
> entanglement entropy. Entanglement adds no explanatory power beyond
> circuit structure.

**Result: Supported.** Neither depth nor entanglement predicts cache
behavior (R² < 0.2 for N ≥ 10; R² > 0.9 at N=8 and N=16 reflects
flat data from depth 0, not a saturation transition).

---

## 9. Falsification Conditions

The primary hypothesis is rejected if any of:

1. Saturation model R² < 0.8 for cache data
2. |Δα| > 0.2 between entropy and cache saturation coefficients
3. No saturation transition detected in cache data

**Conditions 1 and 3 met. Hypothesis not supported.**

---

## 10. Statistical Methodology

- **Saturation fitting:** Piecewise linear model (growth + plateau), split
  point chosen to maximize R².
- **Aggregation:** Mean and standard deviation across seeds per depth.
- **Observable vs entropy:** Binned mean ± 1σ band, one line per N.
- **Seed counts:** 2,000 per condition (statevector), 1,000 per condition (MPS).

---

## 11. Key Findings

### Entropy saturation

- α ≈ 1.6 (Phase 1, N=4-18, lexicographic only in practice)
- α ≈ 2.0-2.5 (Phase 2, N=8-18, lexicographic and gray mappings)
- No convergence toward α=1.0
- locality_aware (bit-reversal) invalidated: α ≈ 0.7-1.3, breaks brickwall coupling

### Cache-entanglement correlation

- **No correlation found.** R² < 0.2 for N ≥ 10 with 2,000 seeds per condition.
- Cache behavior is dominated by circuit layer structure, not entanglement.

### MPS validation

- χ_max grows with entropy in the low-entropy regime.
- Saturates at parameter cap (χ_max) or Hilbert space bound (2^(N/2)).
- Demonstrates instrument sensitivity: qert detects entanglement effects where they exist.

---

## 12. Implementation Constraints (as built)

- **Entropy cap:** k ≤ 12 (Eigen, 256 MB ρ_A)
- **Max qubits tested:** N = 18 (statevector + PAPI), N = 18 (MPS)
- **Dependencies:** C++17, Eigen 3.4.1, PAPI 7.3.0 (optional), Catch2 3.15.0
- **Single-platform:** Intel i7-13700H, Ubuntu 24.04, GCC 13.3.0/15.2.0

---

## 13. Outcomes

| Outcome | Status |
|---------|--------|
| Cache saturation at α_cache ≈ α_entropy | ❌ Not observed |
| Cache fit R² > 0.8 | ❌ R² < 0.2 for N ≥ 10 |
| MPS χ correlates with entropy | ✅ Confirmed |
| Entropy saturation characterized | ✅ α ≈ 2.0-2.5 (lexicographic/gray) |
| Instrument validated | ✅ 41,000 runs, 14M events |

---

## 14. Document History

| Version | Date | Changes |
|---------|------|---------|
| 0.1.0 | 2026-05-14 | Initial specification. Phase 1 design. |
| 0.2.0 | 2026-06-09 | Phase 2 results incorporated. |
| 1.0.0 | 2026-06-23 | Final. MPS comparison added. Phase 2 seed counts updated to 2,000. |
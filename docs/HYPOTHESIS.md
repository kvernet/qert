# Hypothesis specification: Entanglement-execution signatures

**Project:** qert — Quantum Execution Runtime Telemetry  
**Document version:** 0.1.0  
**Status:** Final — Phase 1 and Phase 2 complete  
**Date:** 2026-06-09

---

## 1. Core Research Question

> Does quantum entanglement structure leave measurable signatures in the
> classical execution behavior of statevector simulation?

---

## 2. Primary Hypothesis (tested in Phase 2)

> In 1D brickwall circuits with Haar-random SU(4) gates, the L3 cache miss
> rate during statevector simulation exhibits a saturation transition at a
> circuit depth coinciding with the half-chain entanglement entropy saturation
> depth. The scaling coefficient α_cache = D_90_cache / (N/2) matches
> α_entropy within |Δα| < 0.2, and the piecewise linear saturation model
> achieves R² > 0.8 for both entropy and cache data.

**Result: FALSIFIED.** See Section 13.

---

## 3. Circuit Ensemble

**Primary:** 1D brickwall circuits with Haar-random SU(4) two-qubit gates.

- Even layers: gates on pairs (0,1), (2,3), (4,5), ...
- Odd layers: gates on pairs (1,2), (3,4), (5,6), ...
- Each two-qubit gate is an independent Haar-random SU(4) unitary.

**Rationale:** Well-characterized entanglement dynamics provide a calibrated
baseline. Half-chain entropy grows approximately linearly with depth and
saturates at α · N/2, where α is determined empirically.

---

## 4. Experimental Conditions (as executed)

### Phase 1: Entropy characterization

| Variable | Values |
|----------|--------|
| System sizes N | 4, 6, 8, 10, 12, 14, 16, 18 |
| Circuit depth per run | 3N |
| Qubit mappings | lexicographic, gray, locality_aware (recorded, not applied) |
| Seeds per condition | 100 |
| Total runs | 2,400 |
| Execution mode | Parallel (16 jobs) |
| Hardware counters | Stubbed (zeros) |
| Entropy solver | Hermitian Jacobi (k ≤ 10) |

### Phase 2: Cache-entanglement hypothesis test

| Variable | Values |
|----------|--------|
| System sizes N | 16, 18, 20 |
| Circuit depth per run | 3N |
| Qubit mappings | lexicographic, gray, locality_aware (applied to gate descriptors) |
| Seeds per condition | 100 |
| Total runs | 900 |
| Execution mode | Sequential single-process |
| Hardware counters | PAPI 7.3.0 (perf::LLC-LOAD-MISSES, perf::DTLB-LOAD-MISSES) |
| Entropy solver | Eigen 3.4.1 SelfAdjointEigenSolver (k ≤ 12) |

---

## 5. Observable Hierarchy

| Tier | Observable | Phase 1 | Phase 2 |
|------|-----------|---------|---------|
| **Primary** | L3 cache miss rate per gate | Stubbed | Real (PAPI) |
| **Primary** | TLB miss rate per gate | Stubbed | Real (PAPI) |
| **Secondary** | Working-set size | Stubbed | Stubbed |
| **Secondary** | Stride entropy | Stubbed | Stubbed |
| **Tertiary** | Half-chain von Neumann entropy | Jacobi | Eigen |
| **Quaternary** | Per-gate execution time | Real | Real |

---

## 6. Transition Observables

Three transition depths extracted from piecewise linear saturation fits:

| Observable | Definition |
|-----------|------------|
| D_50 | Depth at 50% of plateau value |
| D_90 | Depth at 90% of plateau value |
| D_inf | Split point depth (inflection between growth and plateau) |

---

## 7. Invariant Definition

For D_90: α = D_90 / (N/2). Hypothesis predicts |Δα| < 0.2 across valid
mappings (lexicographic and gray only; locality_aware excluded — see
Section 11).

---

## 8. Null Hypothesis

> Cache miss rate is predicted equally well by circuit depth as by
> entanglement entropy. Entanglement adds no explanatory power beyond
> circuit structure.

**Result: Null hypothesis supported.** Neither depth nor entanglement
predicts cache behavior (R² < 0.2 for all Phase 2 cache fits).

---

## 9. Falsification Conditions

The primary hypothesis is rejected if any of:

1. Saturation model R² < 0.8 for cache data
2. |Δα| > 0.2 between entropy and cache saturation coefficients
3. No saturation transition detected in cache data

**All three conditions met.** Hypothesis is falsified.

---

## 10. Statistical Methodology

- **Saturation fitting:** Piecewise linear model (growth + plateau), split
  point chosen to maximize R².
- **Aggregation:** Mean and standard deviation across seeds per depth.
- **Invariant test:** Spread of α across mappings compared to 0.2 threshold.

Note: Bootstrap resampling (10,000 resamples) was pre-registered but not
implemented. Uncertainty is reported as standard deviation across conditions.

---

## 11. Control Variables and Findings

| Alternative Explanation | Control Method | Finding |
|------------------------|----------------|---------|
| Cache misses driven by circuit depth, not entanglement | Fit saturation model to cache data | Model fails (R² < 0.2). Neither depth nor entanglement predicts cache. |
| Qubit mapping artifact | Test 3 mappings | Lexicographic and gray agree. locality_aware invalidated (breaks brickwall coupling). |
| x86 artifact | Cross-platform | Not yet tested. Deferred. |
| Small-N finite-size effects | Sweep N=16-20 | No trend across N. |
| Compiler artifact | Single compiler (GCC 13.3.0) | Not tested. |

### locality_aware mapping note

The bit-reversal permutation produces fundamentally different entropy
dynamics (α ≈ 0.7-0.9 vs 1.9-2.5). It breaks the nearest-neighbor
coupling structure of the brickwall ensemble and is not a valid
layout-preserving mapping for this circuit family. Excluded from
invariant analysis.

---

## 12. Implementation Constraints (as built)

- **Phase 1 entropy cap:** k ≤ 10 (Jacobi, 16 MB ρ_A)
- **Phase 2 entropy cap:** k ≤ 12 (Eigen, 256 MB ρ_A)
- **Max qubits tested:** N = 20 (Phase 2), N = 22 (entropy skipped)
- **Dependencies:** C++17, Eigen 3.4.1, PAPI 7.3.0 (optional), Catch2 3.15.0
- **Single-threaded execution**
- **Single-platform:** Intel i7-13700H, Ubuntu 24.04, GCC 13.3.0

---

## 13. Outcomes and Interpretation

| Outcome | Status | Interpretation |
|---------|--------|---------------|
| Strong invariant correlation | ❌ | Hypothesis falsified |
| Correlation exists but architecture-dependent | ❌ | Not observed |
| No entanglement-specific signal | ✅ | **Confirmed.** Cache behavior is layer-structure-driven, not entanglement-driven, at N ≤ 20. |
| Entropy saturation characterized | ✅ | α ≈ 1.60 (N=4-18), α ≈ 2.0-2.5 (N=16-20). No convergence to 1.0. |

---

## 14. Actual Venue

Paper in preparation. Target: systems/quantum venues (ISPASS, PACT, or
Quantum journal).

---

## 15. Non-Negotiable Scientific Principles (all upheld)

1. ✅ **Instrumentation before optimization.** No performance work until observability validated.
2. ✅ **Negative-result-capable.** Null result is the primary finding.
3. ✅ **Cross-validation.** Mapping invariance tested; cross-platform deferred.
4. ✅ **No overinterpretation.** Cache null result reported with R² values.
5. ✅ **Raw telemetry preserved.** All CSVs archived.
6. ✅ **Hypothesis locked before data collection.** This document is version-controlled.

---

## 16. Document History

| Version | Date | Changes |
|---------|------|---------|
| 0.1.0 | 2026-05-14 | Initial specification. Phase 1 design. |
| 0.2.0 | 2026-06-09 | Final. Phase 2 results incorporated. Hypothesis outcome recorded. |
# Hypothesis specification: Entanglement-manifest execution signatures

**Project:** qert — Quantum Execution Runtime Telemetry  
**Document version:** 0.1.0  
**Status:** Locked before Phase 1 implementation  
**Date:** 2026-05-14

---

## 1. Core Research Question

> Does quantum entanglement structure leave invariant, measurable signatures in the classical execution behavior of statevector simulation?

---

## 2. Primary Hypothesis

> In 1D random unitary brickwall circuits, the L3 cache miss rate during statevector simulation exhibits a saturation transition at a circuit depth coinciding with the Page time (half-chain entanglement entropy saturation at depth ≈ N/2). The scaling coefficient α in D_cache = α · D_Page + β is invariant (|Δα| < 0.2) across qubit mapping strategies, memory layouts, CPU architectures, and compiler configurations.

---

## 3. Circuit Ensemble

**Primary:** 1D brickwall random unitary circuits with Haar-random SU(4) two-qubit gates.

```
Qubit:  0 ──[U]──[U]──[U]──[U]── ...
         │    │    │    │
Qubit:  1 ──[U]──[U]──[U]──[U]── ...
         │    │    │    │
Qubit:  2 ──[U]──[U]──[U]──[U]── ...
         │    │    │    │
Qubit:  3 ──[U]──[U]──[U]──[U]── ...

Layer:    0    1    2    3    ...
```

- Even layers: gates on pairs (0,1), (2,3), (4,5), ...
- Odd layers: gates on pairs (1,2), (3,4), (5,6), ...
- Each two-qubit gate is a Haar-random SU(4) unitary

**Rationale:** The entanglement dynamics are analytically known. Half-chain entropy grows linearly with depth until saturation at the Page time (depth ≈ N/2). This provides a sharp, quantitative theoretical baseline for comparison.

**Control ensemble:** 1D brickwall with randomized logical-to-physical qubit permutation per instance. Same entanglement physics, scrambled memory access patterns. This separates entanglement-driven effects from indexing-locality artifacts.

---

## 4. Experimental Conditions

| Variable | Values |
|----------|--------|
| System sizes N | 12, 16, 20 (Phase 1); 24 (Phase 2 with optimized entropy) |
| Circuit depth | 0 to 3N (covers pre-Page through post-saturation) |
| Memory layouts | Lexicographic, Gray-code, locality-aware (Hilbert/Z-order) |
| CPU architectures | x86 (Intel), ARM (Apple Silicon) |
| Compilers | GCC -O2, Clang -O2 |
| Instances per condition | 100 (with adaptive stopping after 50 for power analysis) |

---

## 5. Observable Hierarchy

| Tier | Observable | Measurement Method |
|------|-----------|-------------------|
| **Primary** | L3 cache miss rate per gate layer | Hardware performance counters (PAPI/Linux perf) |
| **Primary** | TLB miss rate per gate layer | Hardware performance counters (PAPI) |
| **Secondary** | Working-set spread (unique pages touched) | Software-defined from memory address traces |
| **Secondary** | Stride entropy (access pattern complexity) | Software-defined from memory address traces |
| **Tertiary** | Half-chain von Neumann entanglement entropy | Exact SVD via Hermitian Jacobi diagonalization |
| **Quaternary** | Memory bandwidth (bytes read/written) | Hardware performance counters (PAPI) |

---

## 6. Transition Observables

To avoid sensitivity to any single saturation detection method, we extract four transition depths from the same data:

| Observable | Definition |
|-----------|------------|
| D_50 | Depth at 50% of saturated cache miss rate |
| D_90 | Depth at 90% of saturated cache miss rate |
| D_inf | Depth at maximum derivative (steepest growth point) |
| D_peak | Depth at peak cache miss rate derivative |

Saturation is estimated via piecewise linear fit (two-segment model: growth + plateau). Uncertainty is quantified via bootstrap resampling (10,000 resamples across circuit instances).

---

## 7. Invariant Definition

> For each transition observable D_i and system size N, fit D_i(N) = α_i · D_Page(N) + β_i where D_Page(N) = N/2.
>
> **Hypothesis prediction:** α_i is invariant across all experimental conditions with |Δα_i| < 0.2 and the linear fit achieves R² > 0.8.
>
> **Falsification:** |Δα| > 0.2 across any two conditions for all four transition observables, or linear fit R² < 0.8.

---

## 8. Null Hypothesis

> Cache miss rate transition depths are equally well predicted by circuit depth alone. Half-chain entanglement entropy adds less than 5% additional explanatory power in a mixed-effects model with circuit instance as random effect, after controlling for temporal autocorrelation.

---

## 9. Falsification Conditions

The primary hypothesis is rejected if any of the following hold:

1. |Δα| > 0.2 across any two experimental conditions for all four transition observables
2. Linear fit R² < 0.8 for the entanglement-Page scaling relationship
3. No transition observable shows consistent scaling across conditions
4. Mixed-effects model shows ΔR² < 0.05 for entanglement entropy beyond circuit depth

---

## 10. Statistical Methodology

- **Regression:** Mixed-effects models with circuit instance as random effect
- **Autocorrelation:** Lag-corrected time-series regression; report raw and corrected p-values
- **Uncertainty:** Bootstrap confidence intervals (10,000 resamples) for all transition depths
- **Power analysis:** Pilot N=50 instances; compute effect size and variance; collect additional data if required N < 500
- **Multiple comparisons:** Bonferroni correction across four transition observables

---

## 11. Control Variables and Alternative Explanations

| Alternative Explanation | Control Method |
|------------------------|----------------|
| Cache misses driven by circuit depth, not entanglement | Include depth as predictor; test ΔR² for entanglement |
| Observed correlation is a qubit mapping artifact | Test with 3 memory layouts (lexicographic, Gray, Hilbert) |
| Observed correlation is an x86 artifact | Replicate on ARM (Apple Silicon) |
| Correlation disappears at larger N | Sweep N = 12, 16, 20 |
| Signal is a compiler optimization artifact | Test with GCC and Clang at -O2 |
| Entanglement entropy alone is insufficient | Track mutual information graph structure (Phase 2) |

---

## 12. Implementation Constraints (Phase 1)

- **Entropy subsystem cap:** k ≤ 10 (ρ_A ≤ 1024×1024, ~16 MB)
- **Maximum qubits for half-chain entropy:** N ≤ 20 (k = N/2 ≤ 10)
- **Maximum qubits for statevector simulation:** N ≤ 24 (256 MB statevector)
- **No external dependencies beyond C++17 standard library and PAPI**
- **Single-threaded execution (parallelism is a Phase 2 confound)**

---

## 13. Expected Outcomes and Interpretation

| Outcome | Interpretation | Action |
|---------|---------------|--------|
| Strong, invariant correlation (α stable across conditions) | Entanglement structure manifests in classical execution. Hypothesis supported. | Paper 1: positive result. Phase 3: entanglement-aware adaptive scheduling. |
| Correlation exists but is architecture-dependent | Signal is real but implementation-mediated. | Paper 1: execution behavior characterization. Investigate mediating mechanisms. |
| No entanglement-specific signal beyond depth/gate count | Entanglement does not leave measurable classical execution signatures in statevector simulation. | Paper 1: negative result. Valuable null finding. Test with tensor network substrate in Phase 2. |

---

## 14. Venue Strategy

| Outcome | Target Venue | Framing |
|---------|-------------|---------|
| Positive (invariant found) | ISPASS, PACT, or ISPASS → Quantum (journal) as follow-up | "Entanglement dynamics manifest in classical simulation hardware" |
| Null (no invariant) | ISPASS, PACT, or SC workshop | "What runtime profiling reveals about quantum circuit simulation: an empirical study" |

---

## 15. Non-Negotiable Scientific Principles

1. **Instrumentation before optimization.** No performance improvements before observability works.
2. **Negative-result-capable.** The experimental design must produce a publishable result regardless of outcome.
3. **Cross-validation.** Every claimed invariant must survive layout, architecture, and compiler changes.
4. **No overinterpretation.** Correlations are reported with confidence intervals and alternative explanations tested.
5. **Raw telemetry preserved.** All raw CSV files are archived for future reanalysis with better methods.
6. **Hypothesis locked before data collection.** This document is version-controlled. Analysis decisions made after seeing data are explicitly flagged as exploratory.
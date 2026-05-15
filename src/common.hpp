#pragma once

#include <complex>
#include <cstdint>
#include <type_traits>

// --- Design Note ---
// All indices and numeric types are aliased rather than used directly.
// This serves three purposes:
//   1. Semantic clarity: StateIndex vs Qubit are distinct concepts.
//   2. Future-proofing: changing StateIndex to uint128_t requires one edit.
//   3. Self-documenting function signatures: the type tells you the meaning.

namespace qert
{

    // --- Fundamental Numeric Types ---

    using Complex = std::complex<double>;
    using Amplitude = Complex;

    // --- Semantic Index Types ---

    using StateIndex = uint64_t; // Index into statevector (0 to 2^N - 1)
    using Qubit = uint32_t;      // Qubit identifier (0 to N-1)
    using GateIndex = uint32_t;  // Position of a gate within a circuit layer
    using Depth = uint32_t;      // Circuit layer index
    using EventID = uint64_t;    // Monotonically increasing telemetry event counter

    // --- Numerical Constants ---

    constexpr double NUMERICAL_EPSILON = 1e-12;

    // --- Platform Assumptions ---

    static_assert(sizeof(Complex) == 16, "Complex must be 128-bit");

    static_assert(sizeof(StateIndex) == 8, "StateIndex must be 64-bit");

} // namespace qert
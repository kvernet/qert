#pragma once

// Low-level quantum gate kernels operating directly on contiguous
// statevector storage.
//
// These kernels are intentionally allocation-free and telemetry-agnostic.
// Instrumentation occurs at higher execution layers. This separation
// ensures that gate application overhead is measurable independently
// of telemetry overhead — critical for the entanglement-execution
// correlation hypothesis.
//
// All functions operate on raw Complex* data, not on Statevector objects.
// This enables direct use in tight loops, independent testing, and
// future extension to batched/fused gate application without API changes.

#include "common.hpp"

#include <array>
#include <random>

namespace qert
{

// --- Random gate generation ---

// Generate a Haar-random SU(4) matrix for two-qubit gates.
// Uses the method described in arXiv:math-ph/0609050:
// generate 4x4 complex matrix with i.i.d. normal entries, QR decompose,
// adjust phases for unit determinant.
//
// The result is a 16-element array in row-major order, basis order:
// |00⟩, |01⟩, |10⟩, |11⟩ where the first qubit (q0) is the least
// significant in the local two-qubit basis.
std::array<Complex, 16> generate_random_su4(std::mt19937_64 &rng);

// --- Single-qubit gates ---

// Apply Hadamard gate to |target⟩.
// H|0⟩ = (|0⟩ + |1⟩)/√2,  H|1⟩ = (|0⟩ - |1⟩)/√2
//
// Preconditions:
//   sv != nullptr
//   target < num_qubits
//   num_qubits <= MAX_QUBITS
void apply_hadamard(Complex *sv, uint32_t num_qubits, Qubit target);

// Apply Pauli-X (NOT) gate to |target⟩.
// X|0⟩ = |1⟩, X|1⟩ = |0⟩
void apply_pauli_x(Complex *sv, uint32_t num_qubits, Qubit target);

// Apply Pauli-Y gate to |target⟩.
// Y|0⟩ = i|1⟩, Y|1⟩ = -i|0⟩
void apply_pauli_y(Complex *sv, uint32_t num_qubits, Qubit target);

// Apply Pauli-Z gate to |target⟩.
// Z|0⟩ = |0⟩, Z|1⟩ = -|1⟩
void apply_pauli_z(Complex *sv, uint32_t num_qubits, Qubit target);

// Apply phase gate S (√Z) to |target⟩.
// Z|0⟩ = |0⟩, Z|1⟩ = i|1⟩
void apply_phase_s(Complex *sv, uint32_t num_qubits, Qubit target);

// Apply phase gate T (π/8) to |target⟩.
// Z|0⟩ = |0⟩, Z|1⟩ = (1+i)/√2|1⟩
void apply_phase_t(Complex *sv, uint32_t num_qubits, Qubit target);

// Apply arbitrary single-qubit unitary.
// Matrix is row-major: [u00, u01, u10, u11] in basis order |0⟩, |1⟩.
void apply_single_qubit_unitary(Complex *sv, uint32_t num_qubits, Qubit target, Complex u00,
                                Complex u01, Complex u10, Complex u11);

// --- Two-qubit gates ---

// Apply controlled-NOT (CNOT) gate.
// |c,t⟩ → |c, t⊕c⟩
//
// Preconditions:
//   sv != nullptr
//   control < num_qubits, target < num_qubits
//   control != target
//   num_qubits <= MAX_QUBITS
void apply_cnot(Complex *sv, uint32_t num_qubits, Qubit control, Qubit target);

// Apply controlled-Z (CZ) gate.
void apply_cz(Complex *sv, uint32_t num_qubits, Qubit control, Qubit target);

// Apply SWAP gate between two qubits.
void apply_swap(Complex *sv, uint32_t num_qubits, Qubit q0, Qubit q1);

// Apply arbitrary two-qubit unitary.
//
// matrix_4x4 is row-major in computational basis order:
//   row/col 0: |00⟩
//   row/col 1: |01⟩
//   row/col 2: |10⟩
//   row/col 3: |11⟩
//
// where q0 is the lower-order qubit in the local two-qubit basis.
// That is: local index = (q0_bit << 0) | (q1_bit << 1).
//
// Preconditions:
//   sv != nullptr
//   q0 != q1
//   q0 < num_qubits, q1 < num_qubits
//   num_qubits <= MAX_QUBITS
//   matrix_4x4 != nullptr
void apply_two_qubit_unitary(Complex *sv, uint32_t num_qubits, Qubit q0, Qubit q1,
                             const Complex *matrix_4x4);

} // namespace qert
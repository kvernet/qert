#pragma once

#include "common.hpp"

namespace qert
{

// --- Entanglement entropy computation ---
//
// Computes the von Neumann entropy of the reduced density matrix
// for a bipartition of the statevector at a given cut point.
//
// The statevector is reshaped into a 2^k × 2^(N-k) matrix where
// k is the number of qubits in subsystem A (qubits 0..k-1).
// The reduced density matrix is ρ_A = Tr_B(|ψ⟩⟨ψ|) = M * M^†
// where M is the reshaped matrix.
//
// Entropy S = -Tr(ρ_A ln ρ_A) = -Σ λ_i ln(λ_i)
// where λ_i are the eigenvalues of ρ_A.
//
// These functions operate on raw Complex* data, following the same
// pattern as the gate layer. They are independent of Statevector
// to allow direct use on the statevector data buffer.

// Compute half-chain entanglement entropy.
// Splits the system at qubit k = N/2 (floor).
// Returns the von Neumann entropy in nats.
//
// For N qubits, the statevector size is 2^N.
// The bipartition splits into subsystems of size k and N-k.
//
// Uses singular value decomposition (SVD) of the reshaped matrix.
// The singular values squared are the eigenvalues of ρ_A.
//
// Preconditions:
//   sv != nullptr
//   num_qubits >= 2
//   num_qubits <= MAX_QUBITS
double compute_half_chain_entropy(const Complex *sv, uint32_t num_qubits);

// Compute entanglement entropy for an arbitrary bipartition.
// k is the number of qubits in subsystem A (qubits 0..k-1).
// Subsystem B contains qubits k..N-1.
//
// Preconditions:
//   sv != nullptr
//   k > 0 && k < num_qubits
//   num_qubits <= MAX_QUBITS
double compute_entropy(const Complex *sv, uint32_t num_qubits, uint32_t k);

} // namespace qert
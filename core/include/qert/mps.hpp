#pragma once

// ============================================================================
// qert::MpsSimulator — Minimal Matrix Product State simulator for 1D chains.
//
// Represents an N-qubit state as a chain of N site tensors connected by bonds.
// Two-qubit gates on adjacent qubits are applied via contraction, gate
// application, and SVD truncation to a maximum bond dimension χ_max.
//
// Key observable: max_bond_dimension() — the MPS analog of cache misses.
// ============================================================================

#include "common.hpp"

#include <Eigen/Dense>

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace qert
{

class MpsSimulator
{
  public:
    // Construct an N-qubit MPS initialized to |0...0⟩.
    // chi_max: maximum bond dimension (controls accuracy/cost tradeoff).
    // Throws if num_qubits < 2 or chi_max < 1.
    MpsSimulator(uint32_t num_qubits, uint32_t chi_max);

    // Apply a two-qubit gate to adjacent qubits (q0, q1).
    // The qubits must be adjacent: |q0 - q1| == 1.
    // matrix_4x4 is row-major in basis order |00⟩, |01⟩, |10⟩, |11⟩.
    // Throws if qubits are not adjacent.
    void apply_two_qubit_gate(Qubit q0, Qubit q1, const Complex *matrix_4x4);

    // --- Observables ---

    // Number of qubits.
    uint32_t num_qubits() const
    {
        return num_qubits_;
    }

    // Maximum bond dimension across all bonds.
    uint32_t max_bond_dimension() const;

    // Average bond dimension across all bonds.
    double avg_bond_dimension() const;

    // Number of singular values truncated in the last gate application.
    uint32_t last_truncated() const
    {
        return last_truncated_;
    }

    // Sum of squared truncated singular values (truncation error).
    double last_truncation_error() const
    {
        return last_truncation_error_;
    }

  private:
    uint32_t num_qubits_;
    uint32_t chi_max_;

    // Site tensors. site_tensors_[i] is stored as an Eigen::MatrixXcd.
    // For site i, the matrix has shape [chi_left * 2, chi_mid] when
    // contracting left-to-right, and [chi_mid, 2 * chi_right] when
    // contracting right-to-left. The storage convention switches
    // depending on the operation.
    std::vector<Eigen::MatrixXcd> site_tensors_;

    // Bond dimensions. bond_dims_[i] is the bond between site i-1 and site i.
    // bond_dims_[0] = bond_dims_[N] = 1 (boundary conditions).
    std::vector<uint32_t> bond_dims_;

    // Truncation tracking from the most recent SVD split.
    uint32_t last_truncated_ = 0;
    double last_truncation_error_ = 0.0;
};

} // namespace qert
#pragma once

#include "common.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace qert
{

// --- Gate descriptor ---
// Describes a single gate in a circuit without storing the gate matrix.
// The matrix is generated at execution time from the RNG seed for
// random unitaries, or looked up for fixed gates.

enum class GateType : uint8_t
{
    HADAMARD,
    PAULI_X,
    PAULI_Y,
    PAULI_Z,
    PHASE_S,
    PHASE_T,
    CNOT,
    CZ,
    SWAP,
    RANDOM_SU4 // Haar-random two-qubit unitary
};

struct GateDescriptor
{
    GateType type;
    Qubit target;       // For single-qubit gates and two-qubit target
    Qubit control;      // For two-qubit gates (unused for single-qubit)
    Depth depth;        // Circuit layer index
    GateIndex gate_idx; // Position within this layer
};

// --- Circuit ---
// An ordered sequence of gates grouped into layers.
// Each layer contains gates that act on disjoint qubits and can be
// executed in any order (or in parallel, in future work).
//
// For 1D brickwall circuits:
//   - Even layers: gates on qubit pairs (0,1), (2,3), (4,5), ...
//   - Odd layers:  gates on qubit pairs (1,2), (3,4), (5,6), ...

class Circuit
{
  public:
    Circuit(uint32_t num_qubits, uint32_t depth);

    // --- Accessors ---
    uint32_t num_qubits() const
    {
        return num_qubits_;
    }
    uint32_t depth() const
    {
        return depth_;
    }
    uint32_t num_gates() const
    {
        return static_cast<uint32_t>(gates_.size());
    }

    // --- Gate access ---
    const GateDescriptor &gate(uint32_t index) const
    {
        return gates_[index];
    }
    const std::vector<GateDescriptor> &gates() const
    {
        return gates_;
    }

    // --- Layer access ---
    // Number of gates in a given layer.
    uint32_t layer_size(Depth d) const;

    // --- Circuit building ---
    // Add a single-qubit gate.
    void add_single_qubit_gate(GateType type, Qubit target, Depth depth, GateIndex gate_idx);

    // Add a two-qubit gate.
    void add_two_qubit_gate(GateType type, Qubit control, Qubit target, Depth depth,
                            GateIndex gate_idx);

    // --- Factory methods ---
    // Build a 1D brickwall circuit with random SU(4) gates.
    // depth: total number of layers.
    static Circuit brickwall_1d(uint32_t num_qubits, uint32_t depth);

    // Apply a qubit-to-memory mapping to all gates in the circuit.
    // Transforms logical qubit indices in-place according to the strategy.
    //   "lexicographic"  — identity (no change)
    //   "gray"           — Gray code permutation
    //   "locality_aware" — Z-order (Morton) curve
    // Throws std::invalid_argument for unknown strategies.
    void apply_qubit_mapping(const std::string &strategy);

  private:
    uint32_t num_qubits_;
    uint32_t depth_;
    std::vector<GateDescriptor> gates_;
    std::vector<uint32_t> layer_sizes_; // gates per layer
};

} // namespace qert
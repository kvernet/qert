#pragma once

#include "common.hpp"
#include <cstddef>
#include <vector>

namespace qert
{

    // Statevector represents an N-qubit pure quantum state as a flat array
    // of 2^N complex amplitudes in lexicographic basis order.
    //
    // Index i corresponds to basis state |i⟩ where i is the binary
    // representation of the qubit configuration (little-endian: LSB = qubit 0).
    //
    // Memory: 2 * 2^N * sizeof(double) = 2^(N+4) bytes.
    // N=24 → 256 MB, N=28 → 4 GB, N=30 → 16 GB.
    // Practical limit for single-machine simulation is ~30 qubits.
    //
    // This is intentionally minimal. No gate application logic lives here.
    // Gates are free functions that operate on raw Complex* data.

    class Statevector
    {
    public:
        // --- Construction ---
        // Creates statevector initialized to |0...0⟩.
        // Throws std::invalid_argument if num_qubits == 0 or > MAX_QUBITS.
        explicit Statevector(uint32_t num_qubits);

        // Statevectors are expensive to copy. Allow moves, forbid copies.
        Statevector(const Statevector &) = delete;
        Statevector &operator=(const Statevector &) = delete;
        Statevector(Statevector &&) noexcept = default;
        Statevector &operator=(Statevector &&) noexcept = default;

        // --- Accessors ---
        uint32_t num_qubits() const { return num_qubits_; }
        StateIndex size() const { return static_cast<StateIndex>(data_.size()); }

        // Amplitude access by state index.
        // amplitude(0) = amplitude of |0...0⟩.
        Complex &amplitude(StateIndex idx);
        const Complex &amplitude(StateIndex idx) const;

        // Raw data access for gate kernels and telemetry.
        Complex *data() { return data_.data(); }
        const Complex *data() const { return data_.data(); }

        // --- Operations ---
        // Reset state to |0...0⟩.
        void reset_to_zero();

        // L2 norm of the statevector. Should be ~1.0 for valid states.
        double norm() const;

    private:
        uint32_t num_qubits_;
        std::vector<Complex> data_; // size = 1 << num_qubits
    };

} // namespace qert
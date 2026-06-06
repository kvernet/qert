#include "qert/circuit.hpp"

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace qert
{

Circuit::Circuit(uint32_t num_qubits, uint32_t depth) : num_qubits_(num_qubits), depth_(depth)
{
    if (num_qubits == 0 || num_qubits > MAX_QUBITS)
    {
        throw std::invalid_argument("num_qubits must be in [1, " + std::to_string(MAX_QUBITS) +
                                    "]");
    }
    if (depth == 0)
    {
        throw std::invalid_argument("depth must be > 0");
    }

    layer_sizes_.resize(depth, 0);
}

uint32_t Circuit::layer_size(Depth d) const
{
    assert(d < depth_);
    return layer_sizes_[d];
}

void Circuit::add_single_qubit_gate(GateType type, Qubit target, Depth depth, GateIndex gate_idx)
{
    assert(depth < depth_);
    assert(target < num_qubits_);

    gates_.push_back({type, target, 0, depth, gate_idx});
    layer_sizes_[depth]++;
}

void Circuit::add_two_qubit_gate(GateType type, Qubit control, Qubit target, Depth depth,
                                 GateIndex gate_idx)
{
    assert(depth < depth_);
    assert(control < num_qubits_);
    assert(target < num_qubits_);
    assert(control != target);

    gates_.push_back({type, target, control, depth, gate_idx});
    layer_sizes_[depth]++;
}

Circuit Circuit::brickwall_1d(uint32_t num_qubits, uint32_t depth)
{
    Circuit circuit(num_qubits, depth);

    for (uint32_t d = 0; d < depth; ++d)
    {
        GateIndex gate_idx = 0;

        if (d % 2 == 0)
        {
            // Even layers: gates on pairs (0,1), (2,3), (4,5), ...
            for (uint32_t q = 0; q + 1 < num_qubits; q += 2)
            {
                circuit.add_two_qubit_gate(GateType::RANDOM_SU4, q, q + 1, d, gate_idx++);
            }
        }
        else
        {
            // Odd layers: gates on pairs (1,2), (3,4), (5,6), ...
            for (uint32_t q = 1; q + 1 < num_qubits; q += 2)
            {
                circuit.add_two_qubit_gate(GateType::RANDOM_SU4, q, q + 1, d, gate_idx++);
            }
        }
    }

    return circuit;
}

namespace
{

// Smallest k such that 2^k >= n.
// Returns 0 when n <= 1.
uint32_t required_bits(uint32_t n)
{
    uint32_t bits = 0;
    uint32_t value = 1;

    while (value < n)
    {
        value <<= 1;
        ++bits;
    }

    return bits;
}

// Gray-order permutation.
//
// Example n = 6:
//
// Gray sequence on 3 bits:
//   0 1 3 2 6 7 5 4
//
// Keep values < 6:
//   0 1 3 2 5 4
//
std::vector<uint32_t> gray_permutation(uint32_t n)
{
    if (n == 0)
    {
        return {};
    }

    const uint32_t bits = required_bits(n);
    const uint32_t domain_size = 1u << bits;

    std::vector<uint32_t> perm;
    perm.reserve(n);

    for (uint32_t i = 0; i < domain_size; ++i)
    {
        const uint32_t gray = i ^ (i >> 1);

        if (gray < n)
        {
            perm.push_back(gray);
        }
    }

    return perm;
}

// Bit-reversal permutation.
//
// Example n = 6:
//
// 3-bit reversals:
//   0->0
//   1->4
//   2->2
//   3->6
//   4->1
//   5->5
//   6->3
//   7->7
//
// Keep values < 6:
//   0 4 2 1 5 3
//
std::vector<uint32_t> bit_reverse_permutation(uint32_t n)
{
    if (n == 0)
    {
        return {};
    }

    const uint32_t bits = required_bits(n);
    const uint32_t domain_size = 1u << bits;

    std::vector<uint32_t> perm;
    perm.reserve(n);

    for (uint32_t i = 0; i < domain_size; ++i)
    {
        uint32_t reversed = 0;

        for (uint32_t b = 0; b < bits; ++b)
        {
            if (i & (1u << b))
            {
                reversed |= (1u << (bits - 1 - b));
            }
        }

        if (reversed < n)
        {
            perm.push_back(reversed);
        }
    }

    return perm;
}

void validate_permutation(const std::vector<uint32_t> &perm, uint32_t n)
{
    if (perm.size() != n)
    {
        throw std::runtime_error("Permutation has incorrect size.");
    }

    std::vector<bool> seen(n, false);

    for (uint32_t p : perm)
    {
        if (p >= n)
        {
            throw std::runtime_error("Permutation contains out-of-range index.");
        }

        if (seen[p])
        {
            throw std::runtime_error("Permutation is not bijective.");
        }

        seen[p] = true;
    }

    for (bool present : seen)
    {
        if (!present)
        {
            throw std::runtime_error("Permutation is incomplete.");
        }
    }
}

} // anonymous namespace

void Circuit::apply_qubit_mapping(const std::string &strategy)
{
    if (strategy == "lexicographic")
    {
        return;
    }

    std::vector<uint32_t> perm;

    if (strategy == "gray")
    {
        perm = gray_permutation(num_qubits_);
    }
    else if (strategy == "locality_aware")
    {
        // Historical name retained for compatibility.
        // Internally this is a bit-reversal permutation.
        perm = bit_reverse_permutation(num_qubits_);
    }
    else
    {
        throw std::invalid_argument("Unknown qubit mapping strategy: " + strategy);
    }

    validate_permutation(perm, num_qubits_);

    for (auto &gate : gates_)
    {
        gate.target = perm[gate.target];

        switch (gate.type)
        {
            case GateType::RANDOM_SU4:
            case GateType::CNOT:
            case GateType::CZ:
            case GateType::SWAP:
                gate.control = perm[gate.control];
                break;

            default:
                break;
        }
    }
}

} // namespace qert
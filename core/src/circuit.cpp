#include "qert/circuit.hpp"

#include <cassert>
#include <stdexcept>

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

} // namespace qert
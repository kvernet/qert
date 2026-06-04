#include "qert/statevector.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace qert
{

Statevector::Statevector(uint32_t num_qubits) : num_qubits_(num_qubits)
{
    if (num_qubits == 0 || num_qubits > MAX_QUBITS)
    {
        throw std::invalid_argument("num_qubits must be in [1, " + std::to_string(MAX_QUBITS) +
                                    "]. "
                                    "N=" +
                                    std::to_string(MAX_QUBITS) + " requires 2^" +
                                    std::to_string(MAX_QUBITS) + " complex amplitudes.");
    }

    // Now safe to allocate.
    data_.resize(1ULL << num_qubits, Complex{0.0, 0.0});
    reset_to_zero();
}

Complex &Statevector::amplitude(StateIndex idx)
{
#ifdef QERT_DEBUG
    assert(idx < data_.size());
#endif
    return data_[idx];
}

const Complex &Statevector::amplitude(StateIndex idx) const
{
#ifdef QERT_DEBUG
    assert(idx < data_.size());
#endif
    return data_[idx];
}

void Statevector::reset_to_zero()
{
    std::fill(data_.begin(), data_.end(), Complex{0.0, 0.0});
    data_[0] = Complex{1.0, 0.0}; // |0...0⟩
}

double Statevector::norm() const
{
    double sum_sq = 0.0;
    for (const auto &amp : data_)
    {
        double mag_sq = amp.real() * amp.real() + amp.imag() * amp.imag();
        sum_sq += mag_sq;
    }
    return std::sqrt(sum_sq);
}

std::string to_binary(uint64_t x, uint32_t bits)
{
    std::string s(bits, '0');

    for (int j = (int)bits - 1; j >= 0; --j)
    {
        s[j] = (x & 1ULL) ? '1' : '0';
        x >>= 1ULL;
    }

    return s;
}

void Statevector::print() const
{
    uint64_t size = 1ULL << num_qubits_;
    for (uint64_t i = 0; i < size; ++i)
    {
        std::cout << amplitude(i) << "|" + to_binary(i, num_qubits_) << ">";
        if (i < size - 1)
        {
            std::cout << " + ";
        }
    }
    std::cout << "\n";
}

} // namespace qert
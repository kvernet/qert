#include "statevector.hpp"

#include <cassert>
#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace qert
{

    Statevector::Statevector(uint32_t num_qubits)
        : num_qubits_(num_qubits), data_(1ULL << num_qubits, Complex{0.0, 0.0})
    {
        if (num_qubits == 0 || num_qubits > MAX_QUBITS)
        {
            throw std::invalid_argument(
                "num_qubits must be in [1, " + std::to_string(MAX_QUBITS) + "]. "
                                                                            "N=" +
                std::to_string(MAX_QUBITS) + " requires 2^" +
                std::to_string(MAX_QUBITS) + " complex amplitudes.");
        }
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

} // namespace qert
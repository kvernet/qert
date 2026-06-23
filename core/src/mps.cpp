#include "qert/mps.hpp"

#include <Eigen/SVD>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <stdexcept>

namespace qert
{

MpsSimulator::MpsSimulator(uint32_t num_qubits, uint32_t chi_max)
    : num_qubits_(num_qubits), chi_max_(chi_max), bond_dims_(num_qubits + 1, 1)
{
    if (num_qubits < 2)
    {
        throw std::invalid_argument("MPS requires at least 2 qubits.");
    }
    if (chi_max < 1)
    {
        throw std::invalid_argument("chi_max must be >= 1.");
    }

    site_tensors_.reserve(num_qubits);
    for (uint32_t i = 0; i < num_qubits; ++i)
    {
        Eigen::MatrixXcd site(2, 1);
        site(0, 0) = Complex(1.0, 0.0);
        site(1, 0) = Complex(0.0, 0.0);
        site_tensors_.push_back(site);
    }
}

void MpsSimulator::apply_two_qubit_gate(Qubit q0, Qubit q1, const Complex *matrix_4x4)
{
    assert(q0 < num_qubits_);
    assert(q1 < num_qubits_);

    uint32_t i = std::min(q0, q1);
    uint32_t j = i + 1;

    if (q0 + q1 != i + j)
    {
        throw std::invalid_argument("MpsSimulator only supports adjacent two-qubit gates.");
    }

    uint32_t chi_left = bond_dims_[i];
    uint32_t chi_mid = bond_dims_[i + 1];
    uint32_t chi_right = bond_dims_[i + 2];

    // --- Step 1: Build the contracted tensor ---
    // theta[a, si, sj, c] = Σ_b site_i[a, si, b] * site_j[b, sj, c]
    // Stored as matrix with rows = (a, si, sj) and columns = c.
    Eigen::MatrixXcd theta = Eigen::MatrixXcd::Zero(static_cast<uint32_t>(chi_left * 4), chi_right);

    for (uint32_t a = 0; a < chi_left; ++a)
    {
        for (uint32_t si = 0; si < 2; ++si)
        {
            for (uint32_t sj = 0; sj < 2; ++sj)
            {
                for (uint32_t c = 0; c < chi_right; ++c)
                {
                    Complex sum(0.0, 0.0);
                    for (uint32_t b = 0; b < chi_mid; ++b)
                    {
                        sum += site_tensors_[i](a * 2 + si, b) *
                               site_tensors_[j](b, sj * chi_right + c);
                    }
                    uint32_t row = a * 4 + si * 2 + sj;
                    theta(row, c) = sum;
                }
            }
        }
    }

    // --- Step 2: Apply the 4×4 gate ---
    Eigen::Matrix4cd gate;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            gate(r, c) = matrix_4x4[r * 4 + c];

    Eigen::MatrixXcd theta_gated(chi_left * 4, chi_right);
    for (uint32_t lr = 0; lr < chi_left; ++lr)
    {
        theta_gated.middleRows(static_cast<uint32_t>(lr * 4), 4) =
            gate * theta.middleRows(static_cast<uint32_t>(lr * 4), 4);
    }

    // --- Step 3: Reshape for SVD ---
    // Combine (a, si) as row index, (sj, c) as column index.
    // This is the standard MPS gauge transformation.
    Eigen::MatrixXcd theta_reshaped(chi_left * 2, 2 * chi_right);

    for (uint32_t a = 0; a < chi_left; ++a)
    {
        for (uint32_t si = 0; si < 2; ++si)
        {
            for (uint32_t sj = 0; sj < 2; ++sj)
            {
                for (uint32_t c = 0; c < chi_right; ++c)
                {
                    uint32_t old_row = a * 4 + si * 2 + sj;
                    uint32_t new_row = a * 2 + si;
                    uint32_t new_col = sj * chi_right + c;
                    theta_reshaped(new_row, new_col) = theta_gated(old_row, c);
                }
            }
        }
    }

    // --- Step 4: SVD and truncation ---
    Eigen::BDCSVD<Eigen::MatrixXcd> svd(theta_reshaped, Eigen::ComputeThinU | Eigen::ComputeThinV);

    uint32_t num_sv = static_cast<uint32_t>(svd.singularValues().size());
    uint32_t new_chi = std::min(num_sv, chi_max_);

    last_truncated_ = num_sv - new_chi;
    last_truncation_error_ = 0.0;
    for (uint32_t k = new_chi; k < num_sv; ++k)
    {
        double sv = svd.singularValues()(k);
        last_truncation_error_ += sv * sv;
    }

    bond_dims_[i + 1] = new_chi;

    // --- Step 5: Rebuild site tensors ---
    // Left site:  U * sqrt(S), shape [chi_left * 2, new_chi].
    // Right site: sqrt(S) * V†, shape [new_chi, 2 * chi_right].
    Eigen::MatrixXcd new_site_i(chi_left * 2, new_chi);
    Eigen::MatrixXcd new_site_j(new_chi, 2 * chi_right);

    for (uint32_t row = 0; row < chi_left * 2; ++row)
    {
        for (uint32_t k = 0; k < new_chi; ++k)
        {
            new_site_i(row, k) = svd.matrixU()(row, k) * std::sqrt(svd.singularValues()(k));
        }
    }

    for (uint32_t k = 0; k < new_chi; ++k)
    {
        double sv_root = std::sqrt(svd.singularValues()(k));
        for (uint32_t col = 0; col < 2 * chi_right; ++col)
        {
            new_site_j(k, col) = sv_root * std::conj(svd.matrixV()(col, k));
        }
    }

    site_tensors_[i] = new_site_i;
    site_tensors_[j] = new_site_j;
}

uint32_t MpsSimulator::max_bond_dimension() const
{
    uint32_t max_chi = 0;
    for (uint32_t i = 0; i <= num_qubits_; ++i)
    {
        max_chi = std::max(max_chi, bond_dims_[i]);
    }
    return max_chi;
}

double MpsSimulator::avg_bond_dimension() const
{
    double sum = 0.0;
    for (uint32_t i = 0; i <= num_qubits_; ++i)
    {
        sum += static_cast<double>(bond_dims_[i]);
    }
    return sum / static_cast<double>(num_qubits_ + 1);
}

} // namespace qert
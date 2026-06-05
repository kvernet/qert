#include "qert/entropy.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <Eigen/Dense>

namespace qert
{

double compute_entropy(const Complex *sv, uint32_t num_qubits, uint32_t k)
{
    assert(sv != nullptr);
    assert(k > 0);
    assert(k < num_qubits);
    assert(num_qubits <= 32);

    if (k > MAX_ENTROPY_SUBSYSTEM)
    {
        throw std::runtime_error("Entropy subsystem too large.");
    }

    uint32_t dim_a = 1u << k;
    uint32_t dim_b = 1u << (num_qubits - k);

    // Build reduced density matrix ρ_A = M * M†.
    // M is dim_a × dim_b, reshaped from the statevector.
    // ρ_A is dim_a × dim_a, Hermitian.
    Eigen::MatrixXcd rho(dim_a, dim_a);

    for (uint32_t i = 0; i < dim_a; ++i)
    {
        for (uint32_t j = i; j < dim_a; ++j)
        {
            std::complex<double> sum(0.0, 0.0);
            for (uint32_t b = 0; b < dim_b; ++b)
            {
                const Complex &m_i = sv[b * dim_a + i];
                const Complex &m_j = sv[b * dim_a + j];
                sum += m_i * std::conj(m_j);
            }
            rho(i, j) = sum;
            if (i != j)
            {
                rho(j, i) = std::conj(sum);
            }
        }
    }

    // Diagonalize ρ_A.
    // SelfAdjointEigenSolver handles Hermitian matrices efficiently.
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXcd> solver(rho);
    if (solver.info() != Eigen::Success)
    {
        throw std::runtime_error("Eigenvalue decomposition failed.");
    }

    const Eigen::VectorXd &eigenvalues = solver.eigenvalues();

    // Compute von Neumann entropy: S = -Σ λ ln(λ).
    double entropy = 0.0;
    double trace = 0.0;

    for (int i = 0; i < eigenvalues.size(); ++i)
    {
        double lambda = eigenvalues(i);

        // Clamp small negative values (numerical noise).
        if (lambda < 0.0 && lambda > -1e-12)
        {
            lambda = 0.0;
        }

        if (lambda > 1e-15)
        {
            entropy -= lambda * std::log(lambda);
            trace += lambda;
        }
    }

    // Renormalize if trace drifted.
    if (std::abs(trace - 1.0) > 1e-8 && trace > 1e-15)
    {
        entropy = 0.0;
        double inv_trace = 1.0 / trace;
        for (int i = 0; i < eigenvalues.size(); ++i)
        {
            double lambda = eigenvalues(i) * inv_trace;
            if (lambda > 1e-15)
            {
                entropy -= lambda * std::log(lambda);
            }
        }
    }

    return entropy;
}

double compute_half_chain_entropy(const Complex *sv, uint32_t num_qubits)
{
    assert(sv != nullptr);
    assert(num_qubits >= 2);
    assert(num_qubits <= 32);

    uint32_t k = num_qubits / 2;
    return compute_entropy(sv, num_qubits, k);
}

} // namespace qert
#include "entropy.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace qert
{

    namespace
    {

        constexpr double EPS = 1e-14;
        constexpr int MAX_JACOBI_SWEEPS = 100;

        // Frobenius norm of off-diagonal elements.
        double offdiag_norm(const std::vector<Complex> &A, uint32_t n)
        {
            double sum = 0.0;
            for (uint32_t i = 0; i < n; ++i)
            {
                for (uint32_t j = i + 1; j < n; ++j)
                {
                    double re = A[i * n + j].real();
                    double im = A[i * n + j].imag();
                    sum += re * re + im * im;
                }
            }
            return std::sqrt(sum);
        }

        // Hermitian Jacobi rotation that annihilates A[p,q].
        //
        // For a Hermitian matrix, the rotation is:
        //   c = cos(θ), s = e^{iφ} sin(θ)
        // where φ = arg(A[p,q]).
        //
        // The formulas for the diagonal update use |A[p,q]| and the rotation
        // parameters, preserving Hermiticity and unitarity.
        void jacobi_rotate(
            std::vector<Complex> &A, uint32_t n,
            uint32_t p, uint32_t q)
        {

            Complex apq = A[p * n + q];
            double apq_abs = std::abs(apq);

            if (apq_abs < EPS)
            {
                return;
            }

            double app = A[p * n + p].real();
            double aqq = A[q * n + q].real();

            // Compute rotation angle.
            // θ satisfies: tan(2θ) = 2|apq| / (aqq - app)
            double theta;
            if (std::abs(aqq - app) < EPS)
            {
                theta = (apq_abs > 0) ? 0.7853981633974483 : 0.0; // π/4
            }
            else
            {
                theta = 0.5 * std::atan2(2.0 * apq_abs, aqq - app);
            }

            double c = std::cos(theta);
            double s_real = std::sin(theta);

            // Phase of off-diagonal element: s = e^{iφ} sin(θ)
            Complex phase = (apq_abs < EPS) ? Complex(1.0, 0.0) : apq / apq_abs;
            Complex s = phase * s_real;

            // --- Update rows and columns p, q ---
            for (uint32_t k = 0; k < n; ++k)
            {
                if (k == p || k == q)
                    continue;

                Complex apk = A[p * n + k];
                Complex aqk = A[q * n + k];

                // [new_apk]   [ c       -conj(s) ] [apk]
                // [new_aqk] = [ s        c       ] [aqk]
                Complex new_apk = c * apk - std::conj(s) * aqk;
                Complex new_aqk = s * apk + c * aqk;

                A[p * n + k] = new_apk;
                A[k * n + p] = std::conj(new_apk);

                A[q * n + k] = new_aqk;
                A[k * n + q] = std::conj(new_aqk);
            }

            // --- Update diagonal and off-diagonal elements ---
            // For Hermitian Jacobi with complex phase φ:
            //   A[p,p] = c² * app + s² * aqq - 2*c*s_real*|apq|
            //   A[q,q] = s² * app + c² * aqq + 2*c*s_real*|apq|
            // where s² = s_real² (the phase cancels in the diagonal terms).

            double s2 = s_real * s_real;
            double c2 = c * c;
            double cs = c * s_real;

            double app_new = c2 * app + s2 * aqq - 2.0 * cs * apq_abs;
            double aqq_new = s2 * app + c2 * aqq + 2.0 * cs * apq_abs;

            A[p * n + p] = Complex(app_new, 0.0);
            A[q * n + q] = Complex(aqq_new, 0.0);

            // Annihilate off-diagonal.
            A[p * n + q] = Complex(0.0, 0.0);
            A[q * n + p] = Complex(0.0, 0.0);
        }

        // Compute eigenvalues using cyclic Jacobi sweeps.
        std::vector<double> hermitian_eigenvalues(std::vector<Complex> A, uint32_t n)
        {
            for (int sweep = 0; sweep < MAX_JACOBI_SWEEPS; ++sweep)
            {
                double off_norm = offdiag_norm(A, n);
                if (off_norm < EPS)
                    break;

                for (uint32_t p = 0; p < n; ++p)
                {
                    for (uint32_t q = p + 1; q < n; ++q)
                    {
                        jacobi_rotate(A, n, p, q);
                    }
                }
            }

            std::vector<double> eigenvalues(n);
            for (uint32_t i = 0; i < n; ++i)
            {
                double lambda = A[i * n + i].real();
                if (lambda < 0.0 && std::abs(lambda) < 1e-12)
                {
                    lambda = 0.0;
                }
                eigenvalues[i] = lambda;
            }

            std::sort(eigenvalues.begin(), eigenvalues.end(), std::greater<double>());
            return eigenvalues;
        }

    } // anonymous namespace

    double compute_entropy(const Complex *sv, uint32_t num_qubits, uint32_t k)
    {
        assert(sv != nullptr);
        assert(k > 0);
        assert(k < num_qubits);
        assert(num_qubits <= 32);

        if (k > MAX_ENTROPY_SUBSYSTEM)
        {
            throw std::runtime_error(
                "Entropy subsystem too large for Phase 1 implementation.");
        }

        uint32_t dim_a = 1u << k;
        uint32_t dim_b = 1u << (num_qubits - k);
        uint64_t matrix_size = static_cast<uint64_t>(dim_a) * dim_a;

        // Build reduced density matrix ρ_A = M * M†.
        std::vector<Complex> rho(matrix_size, Complex(0.0, 0.0));

        for (uint32_t i = 0; i < dim_a; ++i)
        {
            for (uint32_t ip = i; ip < dim_a; ++ip)
            {
                Complex sum{0.0, 0.0};
                for (uint32_t j = 0; j < dim_b; ++j)
                {
                    Complex m_i = sv[j * dim_a + i];
                    Complex m_ip = sv[j * dim_a + ip];
                    sum += m_i * std::conj(m_ip);
                }
                rho[i * dim_a + ip] = sum;
                rho[ip * dim_a + i] = std::conj(sum);
            }
        }

        // Diagonalize.
        std::vector<double> eigenvalues = hermitian_eigenvalues(rho, dim_a);

        // Compute trace and entropy.
        double entropy = 0.0;
        double trace = 0.0;

        for (double lambda : eigenvalues)
        {
            if (lambda <= 1e-15)
                continue;
            entropy -= lambda * std::log(lambda);
            trace += lambda;
        }

        // Renormalize if trace drifted due to floating-point error.
        // This is safe because ρ_A should have trace 1 for a normalized state.
        if (std::abs(trace - 1.0) > 1e-8)
        {
            if (trace > 1e-15)
            {
                // Rescale eigenvalues and recompute entropy.
                entropy = 0.0;
                double inv_trace = 1.0 / trace;
                for (double lambda : eigenvalues)
                {
                    if (lambda <= 1e-15)
                        continue;
                    double normalized_lambda = lambda * inv_trace;
                    entropy -= normalized_lambda * std::log(normalized_lambda);
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
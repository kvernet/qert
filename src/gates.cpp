#include "gates.hpp"

#include <cassert>
#include <cmath>
#include <random>

namespace qert
{

    // --- Internal constants ---
    namespace
    {
        constexpr double SQRT2_INV = 0.7071067811865475; // 1/√2
        constexpr double PI = 3.14159265358979323846;
    }

    namespace
    {

        // Compute 2-norm of a complex vector of length n.
        double norm2(const Complex *v, int n)
        {
            double s = 0.0;
            for (int i = 0; i < n; ++i)
            {
                s += v[i].real() * v[i].real() + v[i].imag() * v[i].imag();
            }
            return std::sqrt(s);
        }

    } // anonymous namespace

    // --- Random SU(4) generation ---
    std::array<Complex, 16> generate_random_su4(std::mt19937_64 &rng)
    {
        // Haar-random SU(4) via Mezzadri (2007, arXiv:math-ph/0609050).
        //
        // Algorithm:
        //   1. Generate 4x4 complex Ginibre matrix A
        //   2. Compute QR decomposition via Householder reflections
        //      Explicitly accumulate Q and R separately for correctness.
        //   3. Multiply column j of Q by conj(R[j,j]) / |R[j,j]|
        //      → Q is now Haar-distributed on U(4)
        //   4. Multiply entire matrix by det(Q)^(-1/4) → SU(4)

        std::normal_distribution<double> normal(0.0, 1.0);

        // Step 1: Generate random 4x4 complex matrix (row-major)
        std::array<Complex, 16> A;
        for (int i = 0; i < 16; ++i)
        {
            A[i] = Complex(normal(rng), normal(rng));
        }

        // Step 2: QR decomposition via Householder reflections.
        // Start with Q = I (row-major 4x4 identity).
        Complex Q[16] = {
            {1, 0}, {0, 0}, {0, 0}, {0, 0},
            {0, 0}, {1, 0}, {0, 0}, {0, 0},
            {0, 0}, {0, 0}, {1, 0}, {0, 0},
            {0, 0}, {0, 0}, {0, 0}, {1, 0}
        };

        // Work on a copy of A (which will become R).
        Complex R[16];
        for (int i = 0; i < 16; ++i)
            R[i] = A[i];

        Complex r_diag[4]; // Store R's diagonal for phase correction

        for (int col = 0; col < 4; ++col)
        {
            // Extract subcolumn x = R[col:4, col]
            Complex x[4];
            for (int i = col; i < 4; ++i)
            {
                x[i - col] = R[i * 4 + col];
            }

            int n = 4 - col;
            double alpha = norm2(x, n);

            if (alpha < 1e-15)
            {
                r_diag[col] = Complex(0.0, 0.0);
                continue;
            }

            // Householder vector v
            Complex x0 = x[0];
            double x0_abs = std::abs(x0);
            Complex phase = (x0_abs < 1e-15) ? Complex(1.0, 0.0)
                                             : x0 / x0_abs;

            // v = x + phase * alpha * e1
            Complex v[4] = {};
            v[0] = x0 + phase * alpha;
            for (int i = 1; i < n; ++i)
            {
                v[i] = x[i];
            }

            // beta = 2 / (v^† v)
            double v_norm_sq = 0.0;
            for (int i = 0; i < n; ++i)
            {
                v_norm_sq += v[i].real() * v[i].real() + v[i].imag() * v[i].imag();
            }
            double beta = 2.0 / v_norm_sq;

            // Store R[col, col] = -phase * alpha
            r_diag[col] = -phase * alpha;

            // Apply Householder reflector to R (from the left)
            for (int j = col; j < 4; ++j)
            {
                Complex dot{0.0, 0.0};
                for (int i = 0; i < n; ++i)
                {
                    dot += std::conj(v[i]) * R[(col + i) * 4 + j];
                }
                Complex scale = beta * dot;
                for (int i = 0; i < n; ++i)
                {
                    R[(col + i) * 4 + j] -= scale * v[i];
                }
            }

            // Apply Householder reflector to Q (from the right)
            Complex v_full[4] = {};
            for (int i = col; i < 4; ++i)
                v_full[i] = v[i - col];

            Complex w[4] = {};
            for (int i = 0; i < 4; ++i)
            {
                for (int k = 0; k < 4; ++k)
                {
                    w[i] += Q[i * 4 + k] * v_full[k];
                }
            }

            for (int i = 0; i < 4; ++i)
            {
                for (int k = 0; k < 4; ++k)
                {
                    Q[i * 4 + k] -= beta * w[i] * std::conj(v_full[k]);
                }
            }
        }

        // Step 3: Phase correction using R's diagonal.
        for (int j = 0; j < 4; ++j)
        {
            Complex rjj = r_diag[j];
            double rjj_abs = std::abs(rjj);
            if (rjj_abs < 1e-15)
                continue;

            Complex phase_correction = std::conj(rjj) / rjj_abs;
            for (int i = 0; i < 4; ++i)
            {
                Q[i * 4 + j] *= phase_correction;
            }
        }

        // Q is now Haar-distributed on U(4).

        // Step 4: Compute determinant and adjust to SU(4).
        Complex det{0.0, 0.0};
        for (int j = 0; j < 4; ++j)
        {
            Complex sub[9];
            int idx = 0;
            for (int r = 1; r < 4; ++r)
            {
                for (int c = 0; c < 4; ++c)
                {
                    if (c == j)
                        continue;
                    sub[idx++] = Q[r * 4 + c];
                }
            }
            Complex subdet = sub[0] * (sub[4] * sub[8] - sub[5] * sub[7]) - 
            sub[1] * (sub[3] * sub[8] - sub[5] * sub[6]) + 
            sub[2] * (sub[3] * sub[7] - sub[4] * sub[6]);

            Complex sign = (j % 2 == 0) ? Complex(1.0, 0.0) : Complex(-1.0, 0.0);
            det += sign * Q[j] * subdet;
        }

        double det_phase = std::atan2(det.imag(), det.real());
        Complex correction = std::polar(1.0, -det_phase / 4.0);

        std::array<Complex, 16> result;
        for (int i = 0; i < 16; ++i)
        {
            result[i] = Q[i] * correction;
        }

        return result;
    }

    // --- Single-qubit gate implementations ---

    void apply_hadamard(Complex *sv, uint32_t num_qubits, Qubit target)
    {
        assert(sv != nullptr);
        assert(target < num_qubits);
        assert(num_qubits <= MAX_QUBITS);

        StateIndex mask = 1ULL << target;
        StateIndex size = 1ULL << num_qubits;

        for (StateIndex i = 0; i < size; ++i)
        {
            if (i & mask)
                continue;

            StateIndex j = i | mask;
            Complex a = sv[i];
            Complex b = sv[j];

            sv[i] = (a + b) * SQRT2_INV;
            sv[j] = (a - b) * SQRT2_INV;
        }
    }

    void apply_pauli_x(Complex *sv, uint32_t num_qubits, Qubit target)
    {
        assert(sv != nullptr);
        assert(target < num_qubits);
        assert(num_qubits <= MAX_QUBITS);

        StateIndex mask = 1ULL << target;
        StateIndex size = 1ULL << num_qubits;

        for (StateIndex i = 0; i < size; ++i)
        {
            if (i & mask)
                continue;
            StateIndex j = i | mask;
            std::swap(sv[i], sv[j]);
        }
    }

    void apply_pauli_y(Complex *sv, uint32_t num_qubits, Qubit target)
    {
        assert(sv != nullptr);
        assert(target < num_qubits);
        assert(num_qubits <= MAX_QUBITS);

        StateIndex mask = 1ULL << target;
        StateIndex size = 1ULL << num_qubits;
        const Complex I{0.0, 1.0};

        for (StateIndex i = 0; i < size; ++i)
        {
            if (i & mask)
                continue;
            StateIndex j = i | mask;
            Complex a = sv[i];
            Complex b = sv[j];

            sv[i] = -I * b;
            sv[j] = I * a;
        }
    }

    void apply_pauli_z(Complex *sv, uint32_t num_qubits, Qubit target)
    {
        assert(sv != nullptr);
        assert(target < num_qubits);
        assert(num_qubits <= MAX_QUBITS);

        StateIndex mask = 1ULL << target;
        StateIndex size = 1ULL << num_qubits;

        for (StateIndex i = 0; i < size; ++i)
        {
            if (i & mask)
            {
                sv[i] = -sv[i];
            }
        }
    }

    void apply_phase_s(Complex *sv, uint32_t num_qubits, Qubit target)
    {
        assert(sv != nullptr);
        assert(target < num_qubits);
        assert(num_qubits <= MAX_QUBITS);

        StateIndex mask = 1ULL << target;
        StateIndex size = 1ULL << num_qubits;
        const Complex I{0.0, 1.0};

        for (StateIndex i = 0; i < size; ++i)
        {
            if (i & mask)
            {
                sv[i] *= I;
            }
        }
    }

    void apply_phase_t(Complex *sv, uint32_t num_qubits, Qubit target)
    {
        assert(sv != nullptr);
        assert(target < num_qubits);
        assert(num_qubits <= MAX_QUBITS);

        StateIndex mask = 1ULL << target;
        StateIndex size = 1ULL << num_qubits;
        const Complex T_PHASE = std::polar(1.0, PI / 4.0);

        for (StateIndex i = 0; i < size; ++i)
        {
            if (i & mask)
            {
                sv[i] *= T_PHASE;
            }
        }
    }

    void apply_single_qubit_unitary(
        Complex *sv, uint32_t num_qubits, Qubit target,
        Complex u00, Complex u01, Complex u10, Complex u11)
    {
        assert(sv != nullptr);
        assert(target < num_qubits);
        assert(num_qubits <= MAX_QUBITS);

        StateIndex mask = 1ULL << target;
        StateIndex size = 1ULL << num_qubits;

        for (StateIndex i = 0; i < size; ++i)
        {
            if (i & mask)
                continue;
            StateIndex j = i | mask;

            Complex a = sv[i];
            Complex b = sv[j];

            sv[i] = u00 * a + u01 * b;
            sv[j] = u10 * a + u11 * b;
        }
    }

    // --- Two-qubit gate implementations ---

    void apply_cnot(Complex *sv, uint32_t num_qubits, Qubit control, Qubit target)
    {
        assert(sv != nullptr);
        assert(control < num_qubits);
        assert(target < num_qubits);
        assert(control != target);
        assert(num_qubits <= MAX_QUBITS);

        StateIndex ctrl_mask = 1ULL << control;
        StateIndex tgt_mask = 1ULL << target;
        StateIndex size = 1ULL << num_qubits;

        for (StateIndex i = 0; i < size; ++i)
        {
            if (!(i & ctrl_mask))
                continue;
            if (i & tgt_mask)
                continue;

            StateIndex j = i | tgt_mask;
            std::swap(sv[i], sv[j]);
        }
    }

    void apply_cz(Complex *sv, uint32_t num_qubits, Qubit control, Qubit target)
    {
        assert(sv != nullptr);
        assert(control < num_qubits);
        assert(target < num_qubits);
        assert(control != target);
        assert(num_qubits <= MAX_QUBITS);

        StateIndex ctrl_mask = 1ULL << control;
        StateIndex tgt_mask = 1ULL << target;
        StateIndex size = 1ULL << num_qubits;

        for (StateIndex i = 0; i < size; ++i)
        {
            if ((i & ctrl_mask) && (i & tgt_mask))
            {
                sv[i] = -sv[i];
            }
        }
    }

    void apply_swap(Complex *sv, uint32_t num_qubits, Qubit q0, Qubit q1)
    {
        assert(sv != nullptr);
        assert(q0 < num_qubits);
        assert(q1 < num_qubits);
        assert(q0 != q1);
        assert(num_qubits <= MAX_QUBITS);

        StateIndex mask0 = 1ULL << q0;
        StateIndex mask1 = 1ULL << q1;
        StateIndex size = 1ULL << num_qubits;

        for (StateIndex i = 0; i < size; ++i)
        {
            if ((i & mask0) || !(i & mask1))
                continue;

            StateIndex j = (i & ~mask1) | mask0;
            std::swap(sv[i], sv[j]);
        }
    }

    void apply_two_qubit_unitary(
        Complex *sv, uint32_t num_qubits, Qubit q0, Qubit q1,
        const Complex *matrix_4x4)
    {
        assert(sv != nullptr);
        assert(q0 < num_qubits);
        assert(q1 < num_qubits);
        assert(q0 != q1);
        assert(num_qubits <= MAX_QUBITS);
        assert(matrix_4x4 != nullptr);

        StateIndex mask0 = 1ULL << q0;
        StateIndex mask1 = 1ULL << q1;
        StateIndex size = 1ULL << num_qubits;

        Complex m00 = matrix_4x4[0];
        Complex m01 = matrix_4x4[1];
        Complex m02 = matrix_4x4[2];
        Complex m03 = matrix_4x4[3];
        Complex m10 = matrix_4x4[4];
        Complex m11 = matrix_4x4[5];
        Complex m12 = matrix_4x4[6];
        Complex m13 = matrix_4x4[7];
        Complex m20 = matrix_4x4[8];
        Complex m21 = matrix_4x4[9];
        Complex m22 = matrix_4x4[10];
        Complex m23 = matrix_4x4[11];
        Complex m30 = matrix_4x4[12];
        Complex m31 = matrix_4x4[13];
        Complex m32 = matrix_4x4[14];
        Complex m33 = matrix_4x4[15];

        for (StateIndex i = 0; i < size; ++i)
        {
            if ((i & mask0) || (i & mask1))
                continue;

            StateIndex i01 = i | mask1;
            StateIndex i10 = i | mask0;
            StateIndex i11 = i | mask0 | mask1;

            Complex a00 = sv[i];
            Complex a01 = sv[i01];
            Complex a10 = sv[i10];
            Complex a11 = sv[i11];

            sv[i] = m00 * a00 + m01 * a01 + m02 * a10 + m03 * a11;
            sv[i01] = m10 * a00 + m11 * a01 + m12 * a10 + m13 * a11;
            sv[i10] = m20 * a00 + m21 * a01 + m22 * a10 + m23 * a11;
            sv[i11] = m30 * a00 + m31 * a01 + m32 * a10 + m33 * a11;
        }
    }

} // namespace qert
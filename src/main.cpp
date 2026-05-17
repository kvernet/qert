#include "common.hpp"
#include "seed.hpp"
#include "statevector.hpp"
#include "gates.hpp"
#include <build_info.hpp>

#include <cstdio>
#include <cmath>
#include <random>

// --- Test Helpers ---
namespace
{

    bool nearly_equal(double a, double b, double eps = 1e-10)
    {
        return std::abs(a - b) < eps;
    }

    bool nearly_equal(qert::Complex a, qert::Complex b, double eps = 1e-10)
    {
        return std::abs(a.real() - b.real()) < eps &&
               std::abs(a.imag() - b.imag()) < eps;
    }

} // anonymous namespace

int main()
{
    // ================================================================
    // Seed Tests
    // ================================================================
    qert::set_rng_seed(42);
    if (qert::get_rng_seed() != 42)
    {
        std::fprintf(stderr, "FAIL: seed roundtrip\n");
        return 1;
    }

    qert::RunMetadata meta(
        "brickwall_1d", 16, 48, "lexicographic", 42,
        qert::GIT_COMMIT,
        std::string(qert::COMPILER_ID) + "-" + qert::COMPILER_VERSION,
        "-O2", "x86_64", "test-cpu", 0);

    std::string json = meta.to_json();
    std::printf("%s\n", json.c_str());
    meta.write_to_file("test_metadata.json");

    qert::RunMetadata meta2(
        "brickwall_1d", 16, 48, "lexicographic", 42,
        qert::GIT_COMMIT,
        std::string(qert::COMPILER_ID) + "-" + qert::COMPILER_VERSION,
        "-O2", "x86_64", "test-cpu", 1);

    if (meta.physics_hash != meta2.physics_hash)
    {
        std::fprintf(stderr, "FAIL: physics_hash not stable\n");
        return 1;
    }
    if (meta.full_hash == meta2.full_hash)
    {
        std::fprintf(stderr, "FAIL: full_hash should differ\n");
        return 1;
    }
    std::printf("Seed tests passed.\n\n");

    // ================================================================
    // Statevector Tests
    // ================================================================
    qert::Statevector sv(3); // 3 qubits, 8 amplitudes
    if (sv.num_qubits() != 3)
    {
        std::fprintf(stderr, "FAIL: num_qubits\n");
        return 1;
    }
    if (sv.size() != 8)
    {
        std::fprintf(stderr, "FAIL: size\n");
        return 1;
    }
    // Initial state |000⟩
    if (!nearly_equal(sv.amplitude(0), qert::Complex{1.0, 0.0}))
    {
        std::fprintf(stderr, "FAIL: initial state |000⟩\n");
        return 1;
    }
    if (!nearly_equal(sv.amplitude(1), qert::Complex{0.0, 0.0}))
    {
        std::fprintf(stderr, "FAIL: amplitude at |001⟩ should be 0\n");
        return 1;
    }
    if (!nearly_equal(sv.norm(), 1.0))
    {
        std::fprintf(stderr, "FAIL: norm\n");
        return 1;
    }
    // Reset
    sv.amplitude(3) = qert::Complex{0.5, 0.0};
    sv.reset_to_zero();
    if (!nearly_equal(sv.amplitude(3), qert::Complex{0.0, 0.0}))
    {
        std::fprintf(stderr, "FAIL: reset_to_zero\n");
        return 1;
    }
    // Move semantics
    qert::Statevector sv2(std::move(sv));
    if (sv2.num_qubits() != 3)
    {
        std::fprintf(stderr, "FAIL: move construction\n");
        return 1;
    }
    std::printf("Statevector tests passed.\n\n");

    // ================================================================
    // Gate Tests (N=3, single qubit gates)
    // ================================================================
    qert::Statevector q(3);

    // Test Hadamard: H|0⟩ = (|0⟩+|1⟩)/√2
    q.reset_to_zero();
    qert::apply_hadamard(q.data(), q.num_qubits(), 0);
    if (!nearly_equal(q.amplitude(0), qert::Complex{0.5, 0.0} * std::sqrt(2.0) /* 1/√2 */))
    {
        // amplitude of |000⟩ after H on qubit 0
        double expected = 1.0 / std::sqrt(2.0);
        if (!nearly_equal(std::abs(q.amplitude(0)), expected))
        {
            std::fprintf(stderr, "FAIL: Hadamard |000⟩ amplitude\n");
            return 1;
        }
    }
    if (!nearly_equal(std::abs(q.amplitude(1)), 1.0 / std::sqrt(2.0)))
    {
        std::fprintf(stderr, "FAIL: Hadamard |001⟩ amplitude\n");
        return 1;
    }
    if (!nearly_equal(q.norm(), 1.0))
    {
        std::fprintf(stderr, "FAIL: Hadamard norm\n");
        return 1;
    }

    // Test Pauli-X: X|0⟩ = |1⟩
    q.reset_to_zero();
    qert::apply_pauli_x(q.data(), q.num_qubits(), 1);
    if (!nearly_equal(std::abs(q.amplitude(0)), 0.0))
    {
        std::fprintf(stderr, "FAIL: X|0⟩ should not have |000⟩ component\n");
        return 1;
    }
    if (!nearly_equal(std::abs(q.amplitude(2)), 1.0))
    { // bit 1 set → index 2
        std::fprintf(stderr, "FAIL: X|0⟩ → |010⟩\n");
        return 1;
    }

    // Test Pauli-Z: Z|1⟩ = -|1⟩
    q.reset_to_zero();
    qert::apply_pauli_x(q.data(), q.num_qubits(), 0); // |000⟩ → |001⟩
    qert::apply_pauli_z(q.data(), q.num_qubits(), 0); // |001⟩ → -|001⟩
    if (!nearly_equal(q.amplitude(1), qert::Complex{-1.0, 0.0}))
    {
        std::fprintf(stderr, "FAIL: ZX|0⟩ = -|1⟩\n");
        return 1;
    }

    // Test Pauli-Y: Y|0⟩ = i|1⟩
    q.reset_to_zero();
    qert::apply_pauli_y(q.data(), q.num_qubits(), 0);
    if (!nearly_equal(q.amplitude(0), qert::Complex{0.0, 0.0}))
    {
        std::fprintf(stderr, "FAIL: Y|0⟩ should have zero |000⟩\n");
        return 1;
    }
    if (!nearly_equal(q.amplitude(1), qert::Complex{0.0, 1.0}))
    {
        std::fprintf(stderr, "FAIL: Y|0⟩ = i|1⟩\n");
        return 1;
    }

    // Test H then H = I
    q.reset_to_zero();
    qert::apply_hadamard(q.data(), q.num_qubits(), 0);
    qert::apply_hadamard(q.data(), q.num_qubits(), 0);
    if (!nearly_equal(q.amplitude(0), qert::Complex{1.0, 0.0}))
    {
        std::fprintf(stderr, "FAIL: H*H = I\n");
        return 1;
    }

    std::printf("Single-qubit gate tests passed.\n\n");

    // ================================================================
    // Two-Qubit Gate Tests (N=3)
    // ================================================================

    // Test CNOT: |10⟩ → |11⟩ (control=1, target=0)
    // Prepare |010⟩ = qubit 1 = |1⟩
    q.reset_to_zero();
    qert::apply_pauli_x(q.data(), q.num_qubits(), 1); // |000⟩ → |010⟩
    // CNOT control=1, target=0: |010⟩ → |011⟩ (both bits set)
    qert::apply_cnot(q.data(), q.num_qubits(), 1, 0);
    if (!nearly_equal(std::abs(q.amplitude(3)), 1.0))
    { // |011⟩ = index 3
        std::fprintf(stderr, "FAIL: CNOT |010⟩ → |011⟩\n");
        return 1;
    }
    if (!nearly_equal(std::abs(q.amplitude(2)), 0.0))
    {
        std::fprintf(stderr, "FAIL: CNOT should not leave |010⟩\n");
        return 1;
    }

    // Test CNOT again (should return to original)
    qert::apply_cnot(q.data(), q.num_qubits(), 1, 0);
    if (!nearly_equal(std::abs(q.amplitude(2)), 1.0))
    {
        std::fprintf(stderr, "FAIL: CNOT*CNOT = I\n");
        return 1;
    }

    // Test CZ: |11⟩ → -|11⟩
    q.reset_to_zero();
    qert::apply_pauli_x(q.data(), q.num_qubits(), 0); // |001⟩
    qert::apply_pauli_x(q.data(), q.num_qubits(), 1); // |011⟩
    qert::apply_cz(q.data(), q.num_qubits(), 1, 0);
    if (!nearly_equal(q.amplitude(3), qert::Complex{-1.0, 0.0}))
    {
        std::fprintf(stderr, "FAIL: CZ|11⟩ = -|11⟩\n");
        return 1;
    }

    // Test SWAP: |01⟩ ↔ |10⟩
    q.reset_to_zero();
    qert::apply_pauli_x(q.data(), q.num_qubits(), 0); // |001⟩
    qert::apply_swap(q.data(), q.num_qubits(), 0, 1); // swap bits 0 and 1
    if (!nearly_equal(std::abs(q.amplitude(2)), 1.0))
    { // |010⟩
        std::fprintf(stderr, "FAIL: SWAP |001⟩ → |010⟩\n");
        return 1;
    }

    std::printf("Two-qubit gate tests passed.\n\n");

    // ================================================================
    // Random SU(4) Tests
    // ================================================================
    std::mt19937_64 rng(12345);

    // Generate and test unitarity
    auto mat = qert::generate_random_su4(rng);

    // Check U†U ≈ I
    for (int r = 0; r < 4; ++r)
    {
        for (int c = 0; c < 4; ++c)
        {
            qert::Complex dot{0.0, 0.0};
            for (int k = 0; k < 4; ++k)
            {
                // conj(U[k*4+r]) * U[k*4+c] = (U†)[r,k] * U[k,c]
                dot += std::conj(mat[k * 4 + r]) * mat[k * 4 + c];
            }
            qert::Complex expected = (r == c) ? qert::Complex{1.0, 0.0} : qert::Complex{0.0, 0.0};
            if (!nearly_equal(dot, expected, 1e-8))
            {
                std::fprintf(stderr, "FAIL: SU(4) unitarity at (%d,%d)\n", r, c);
                return 1;
            }
        }
    }
    std::printf("SU(4) unitarity test passed.\n");

    // Test norm preservation under random SU(4) application
    q.reset_to_zero();
    qert::apply_hadamard(q.data(), q.num_qubits(), 0);
    qert::apply_hadamard(q.data(), q.num_qubits(), 1);
    double norm_before = q.norm();
    qert::apply_two_qubit_unitary(q.data(), q.num_qubits(), 0, 1, mat.data());
    double norm_after = q.norm();
    if (!nearly_equal(norm_before, norm_after))
    {
        std::fprintf(stderr, "FAIL: norm not preserved under SU(4)\n");
        return 1;
    }
    std::printf("SU(4) norm preservation test passed.\n\n");

    // ================================================================
    // Inverse Consistency Test
    // ================================================================
    q.reset_to_zero();
    qert::apply_hadamard(q.data(), q.num_qubits(), 0); // prepare non-trivial state

    // Copy state
    qert::Statevector q_copy(3);
    for (qert::StateIndex i = 0; i < q.size(); ++i)
    {
        q_copy.amplitude(i) = q.amplitude(i);
    }

    // Generate another random SU(4)
    auto mat2 = qert::generate_random_su4(rng);

    // Compute U† (conjugate transpose)
    qert::Complex mat2_dagger[16];
    for (int r = 0; r < 4; ++r)
    {
        for (int c = 0; c < 4; ++c)
        {
            mat2_dagger[r * 4 + c] = std::conj(mat2[c * 4 + r]);
        }
    }

    // Apply U then U†
    qert::apply_two_qubit_unitary(q.data(), q.num_qubits(), 0, 1, mat2.data());
    qert::apply_two_qubit_unitary(q.data(), q.num_qubits(), 0, 1, mat2_dagger);

    // Should recover original state
    for (qert::StateIndex i = 0; i < q.size(); ++i)
    {
        if (!nearly_equal(q.amplitude(i), q_copy.amplitude(i), 1e-8))
        {
            std::fprintf(stderr, "FAIL: U†U = I at index %llu\n",
                         (unsigned long long)i);
            return 1;
        }
    }
    std::printf("Inverse consistency test passed.\n\n");

    // ================================================================
    std::printf("All tests passed.\n");
    return 0;
}
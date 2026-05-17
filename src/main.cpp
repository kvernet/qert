#include "common.hpp"
#include "seed.hpp"
#include "statevector.hpp"
#include "gates.hpp"
#include "telemetry.hpp"
#include "entropy.hpp"
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
    // Telemetry Tests
    // ================================================================

    // Test 1: Basic record and file output
    std::string metadata = "{ \"test\": true }";
    qert::TelemetryRecorder recorder("test_telemetry.csv", metadata);

    qert::TelemetryEvent ev;
    ev.event_id = 0;
    ev.depth = 0;
    ev.gate_idx = 0;
    ev.execution_time_ns = 1234;
    ev.l3_misses_delta = 42;
    ev.tlb_misses_delta = 3;
    ev.working_set_kb = 64;
    ev.stride_entropy = 2.5;
    ev.half_chain_entropy = std::nan(""); // not sampled

    recorder.record(ev);

    ev.event_id = 1;
    ev.depth = 0;
    ev.gate_idx = 1;
    ev.execution_time_ns = 1189;
    ev.l3_misses_delta = 38;
    ev.tlb_misses_delta = 2;
    ev.working_set_kb = 64;
    ev.stride_entropy = 2.3;
    ev.half_chain_entropy = 0.693147180560; // ln(2) - sampled at even layer

    recorder.record(ev);

    if (recorder.event_count() != 2)
    {
        std::fprintf(stderr, "FAIL: telemetry event count\n");
        return 1;
    }

    recorder.close();

    // Test 2: Verify file exists and has correct structure
    std::FILE *f = std::fopen("test_telemetry.csv", "r");
    if (!f)
    {
        std::fprintf(stderr, "FAIL: telemetry file not created\n");
        return 1;
    }

    char line[512];

    // Line 1: metadata comment
    if (!std::fgets(line, sizeof(line), f))
    {
        std::fprintf(stderr, "FAIL: no metadata line\n");
        return 1;
    }
    if (line[0] != '#')
    {
        std::fprintf(stderr, "FAIL: metadata line missing '#' prefix\n");
        return 1;
    }

    // Line 2: CSV header
    if (!std::fgets(line, sizeof(line), f))
    {
        std::fprintf(stderr, "FAIL: no header line\n");
        return 1;
    }
    if (std::string(line).find("event_id") == std::string::npos)
    {
        std::fprintf(stderr, "FAIL: header missing 'event_id'\n");
        return 1;
    }

    // Line 3: first data row (NaN entropy)
    if (!std::fgets(line, sizeof(line), f))
    {
        std::fprintf(stderr, "FAIL: no first data row\n");
        return 1;
    }
    if (std::string(line).find("nan") == std::string::npos)
    {
        std::fprintf(stderr, "FAIL: first row should contain 'nan'\n");
        return 1;
    }

    // Line 4: second data row (numeric entropy)
    if (!std::fgets(line, sizeof(line), f))
    {
        std::fprintf(stderr, "FAIL: no second data row\n");
        return 1;
    }

    std::fclose(f);
    std::printf("Telemetry tests passed.\n\n");

    // ================================================================
    // Entropy Tests
    // ================================================================

    // Test 1: |0...0⟩ state has zero entropy (N=4, half-chain k=2)
    qert::Statevector q_zero(4);
    double s0 = qert::compute_half_chain_entropy(q_zero.data(), q_zero.num_qubits());
    if (std::abs(s0) > 1e-10)
    {
        std::fprintf(stderr, "FAIL: |0⟩ entropy = %.12f, expected 0\n", s0);
        return 1;
    }
    std::printf("Entropy test 1 passed: |0...0⟩ has S=0\n");

    // Test 2: Bell state (N=2, half-chain k=1) has entropy ln(2)
    // Prepare: H on qubit 0, then CNOT control=0 target=1
    // State: (|00⟩ + |11⟩)/√2
    // Half-chain splits at k=1: qubit 0 in A, qubit 1 in B → S = ln(2)
    qert::Statevector q_bell(2);
    q_bell.reset_to_zero();
    qert::apply_hadamard(q_bell.data(), q_bell.num_qubits(), 0);
    qert::apply_cnot(q_bell.data(), q_bell.num_qubits(), 0, 1);

    double s_bell = qert::compute_half_chain_entropy(q_bell.data(), q_bell.num_qubits());
    double expected_bell = std::log(2.0); // ≈ 0.693147180560
    if (std::abs(s_bell - expected_bell) > 1e-6)
    {
        std::fprintf(stderr, "FAIL: Bell state entropy = %.12f, expected %.12f\n",
                     s_bell, expected_bell);
        return 1;
    }
    std::printf("Entropy test 2 passed: Bell state has S=ln(2)=%.12f\n", s_bell);

    // Test 3: Product state |+⟩⊗|+⟩ (N=2) has zero half-chain entropy
    q_bell.reset_to_zero();
    qert::apply_hadamard(q_bell.data(), q_bell.num_qubits(), 0);
    qert::apply_hadamard(q_bell.data(), q_bell.num_qubits(), 1);
    double s_product = qert::compute_half_chain_entropy(q_bell.data(), q_bell.num_qubits());
    if (std::abs(s_product) > 1e-10)
    {
        std::fprintf(stderr, "FAIL: product state entropy = %.12f, expected 0\n", s_product);
        return 1;
    }
    std::printf("Entropy test 3 passed: product state has S=0\n");

    // Test 4: GHZ state (N=3, half-chain k=1) has entropy ln(2)
    // |000⟩ + |111⟩, half-chain splits at k=1: qubit 0 in A, qubits 1,2 in B
    qert::Statevector q_ghz(3);
    q_ghz.reset_to_zero();
    qert::apply_hadamard(q_ghz.data(), q_ghz.num_qubits(), 0);
    qert::apply_cnot(q_ghz.data(), q_ghz.num_qubits(), 0, 1);
    qert::apply_cnot(q_ghz.data(), q_ghz.num_qubits(), 0, 2);
    double s_ghz = qert::compute_half_chain_entropy(q_ghz.data(), q_ghz.num_qubits());
    if (std::abs(s_ghz - expected_bell) > 1e-6)
    {
        std::fprintf(stderr, "FAIL: GHZ entropy = %.12f, expected %.12f\n",
                     s_ghz, expected_bell);
        return 1;
    }
    std::printf("Entropy test 4 passed: GHZ state has S=ln(2)=%.12f\n", s_ghz);

    // Test 5: Entropy grows with circuit depth (N=6, random brickwall)
    qert::Statevector q_rand(6);
    q_rand.reset_to_zero();

    std::mt19937_64 rng_ent(42);

    double prev_entropy = 0.0;
    bool entropy_grew = false;

    for (int layer = 0; layer < 20; ++layer)
    {
        if (layer % 2 == 0)
        {
            for (uint32_t q = 0; q < 5; q += 2)
            {
                auto mat = qert::generate_random_su4(rng_ent);
                qert::apply_two_qubit_unitary(q_rand.data(), q_rand.num_qubits(),
                                              q, q + 1, mat.data());
            }
        }
        else
        {
            for (uint32_t q = 1; q < 5; q += 2)
            {
                auto mat = qert::generate_random_su4(rng_ent);
                qert::apply_two_qubit_unitary(q_rand.data(), q_rand.num_qubits(),
                                              q, q + 1, mat.data());
            }
        }

        double s = qert::compute_half_chain_entropy(q_rand.data(), q_rand.num_qubits());

        if (s > prev_entropy + 1e-10)
        {
            entropy_grew = true;
        }
        prev_entropy = s;
    }

    if (!entropy_grew)
    {
        std::fprintf(stderr, "FAIL: entropy did not grow during random circuit\n");
        return 1;
    }
    std::printf("Entropy test 5 passed: entropy grows with random circuit depth\n");

    // Test 6: entropy bound check
    double max_entropy = 3.0 * std::log(2.0); // k=3, max = 3*ln(2) ≈ 2.079
    if (prev_entropy > max_entropy + 1e-10)
    {
        std::fprintf(stderr, "FAIL: entropy %.12f exceeds bound %.12f\n",
                     prev_entropy, max_entropy);
        return 1;
    }
    std::printf("Entropy test 6 passed: entropy within theoretical bounds\n");

    // Test 7: Bell state with explicit bipartition k=1 (N=4)
    // Entangle qubits 0 and 1, leave 2 and 3 in |0⟩
    // Bipartition at k=1: A={0}, B={1,2,3}
    qert::Statevector q_bell4(4);
    q_bell4.reset_to_zero();
    qert::apply_hadamard(q_bell4.data(), q_bell4.num_qubits(), 0);
    qert::apply_cnot(q_bell4.data(), q_bell4.num_qubits(), 0, 1);
    double s_bell4_k1 = qert::compute_entropy(q_bell4.data(), q_bell4.num_qubits(), 1);
    if (std::abs(s_bell4_k1 - expected_bell) > 1e-6)
    {
        std::fprintf(stderr, "FAIL: Bell N=4 k=1 entropy = %.12f, expected %.12f\n",
                     s_bell4_k1, expected_bell);
        return 1;
    }
    std::printf("Entropy test 7 passed: Bell state k=1 has S=ln(2)\n");

    std::printf("Entropy tests passed.\n\n");

    // ================================================================
    std::printf("All tests passed.\n");
    return 0;
}
#include <catch_amalgamated.hpp>
#include "gates.hpp"
#include "statevector.hpp"
#include <random>

TEST_CASE("Hadamard gate", "[gates]")
{
    qert::Statevector sv(2);

    // H|0⟩ = (|0⟩+|1⟩)/√2
    qert::apply_hadamard(sv.data(), sv.num_qubits(), 0);

    double inv_sqrt2 = 1.0 / std::sqrt(2.0);
    REQUIRE(std::abs(sv.amplitude(0) - qert::Complex{inv_sqrt2, 0.0}) < 1e-12);
    REQUIRE(std::abs(sv.amplitude(1) - qert::Complex{inv_sqrt2, 0.0}) < 1e-12);
    REQUIRE(std::abs(sv.norm() - 1.0) < 1e-12);

    // H*H = I
    qert::apply_hadamard(sv.data(), sv.num_qubits(), 0);
    REQUIRE(std::abs(sv.amplitude(0) - qert::Complex{1.0, 0.0}) < 1e-12);
    REQUIRE(std::abs(sv.amplitude(1)) < 1e-12);
}

TEST_CASE("Pauli-X gate", "[gates]")
{
    qert::Statevector sv(2);

    // X|0⟩ = |1⟩
    qert::apply_pauli_x(sv.data(), sv.num_qubits(), 0);
    REQUIRE(std::abs(sv.amplitude(0)) < 1e-12);
    REQUIRE(std::abs(sv.amplitude(1) - qert::Complex{1.0, 0.0}) < 1e-12);
}

TEST_CASE("Pauli-Y gate", "[gates]")
{
    qert::Statevector sv(2);
    const qert::Complex I{0.0, 1.0};

    // Y|0⟩ = i|1⟩
    qert::apply_pauli_y(sv.data(), sv.num_qubits(), 0);
    REQUIRE(std::abs(sv.amplitude(0)) < 1e-12);
    REQUIRE(std::abs(sv.amplitude(1) - I) < 1e-12);
}

TEST_CASE("Pauli-Z gate", "[gates]")
{
    qert::Statevector sv(2);

    // Prepare |1⟩ then Z|1⟩ = -|1⟩
    qert::apply_pauli_x(sv.data(), sv.num_qubits(), 0);
    qert::apply_pauli_z(sv.data(), sv.num_qubits(), 0);
    REQUIRE(std::abs(sv.amplitude(1) + qert::Complex{1.0, 0.0}) < 1e-12);
}

TEST_CASE("CNOT gate", "[gates]")
{
    qert::Statevector sv(2);

    // CNOT|00⟩ = |00⟩
    qert::apply_cnot(sv.data(), sv.num_qubits(), 0, 1);
    REQUIRE(std::abs(sv.amplitude(0) - qert::Complex{1.0, 0.0}) < 1e-12);

    // Prepare |10⟩, CNOT|10⟩ = |11⟩
    sv.reset_to_zero();
    qert::apply_pauli_x(sv.data(), sv.num_qubits(), 0);
    qert::apply_cnot(sv.data(), sv.num_qubits(), 0, 1);
    REQUIRE(std::abs(sv.amplitude(3) - qert::Complex{1.0, 0.0}) < 1e-12);
}

TEST_CASE("CNOT is self-inverse", "[gates]")
{
    qert::Statevector sv(2);
    qert::apply_hadamard(sv.data(), sv.num_qubits(), 0);

    qert::Statevector sv_copy(2);
    for (qert::StateIndex i = 0; i < sv.size(); ++i)
    {
        sv_copy.amplitude(i) = sv.amplitude(i);
    }

    qert::apply_cnot(sv.data(), sv.num_qubits(), 0, 1);
    qert::apply_cnot(sv.data(), sv.num_qubits(), 0, 1);

    for (qert::StateIndex i = 0; i < sv.size(); ++i)
    {
        REQUIRE(std::abs(sv.amplitude(i) - sv_copy.amplitude(i)) < 1e-12);
    }
}

TEST_CASE("CZ gate", "[gates]")
{
    qert::Statevector sv(2);

    // Prepare |11⟩, CZ|11⟩ = -|11⟩
    qert::apply_pauli_x(sv.data(), sv.num_qubits(), 0);
    qert::apply_pauli_x(sv.data(), sv.num_qubits(), 1);
    qert::apply_cz(sv.data(), sv.num_qubits(), 0, 1);
    REQUIRE(std::abs(sv.amplitude(3) + qert::Complex{1.0, 0.0}) < 1e-12);
}

TEST_CASE("SWAP gate", "[gates]")
{
    qert::Statevector sv(2);

    // |01⟩ SWAP → |10⟩
    qert::apply_pauli_x(sv.data(), sv.num_qubits(), 1);
    qert::apply_swap(sv.data(), sv.num_qubits(), 0, 1);
    REQUIRE(std::abs(sv.amplitude(1) - qert::Complex{1.0, 0.0}) < 1e-12);
}

TEST_CASE("SU(4) unitarity", "[gates]")
{
    std::mt19937_64 rng(42);

    for (int trial = 0; trial < 10; ++trial)
    {
        auto mat = qert::generate_random_su4(rng);

        // Check U†U = I
        for (int r = 0; r < 4; ++r)
        {
            for (int c = 0; c < 4; ++c)
            {
                qert::Complex dot{0.0, 0.0};
                for (int k = 0; k < 4; ++k)
                {
                    dot += std::conj(mat[k * 4 + r]) * mat[k * 4 + c];
                }
                qert::Complex expected = (r == c) ? qert::Complex{1.0, 0.0}
                                                  : qert::Complex{0.0, 0.0};
                REQUIRE(std::abs(dot - expected) < 1e-10);
            }
        }
    }
}

TEST_CASE("SU(4) norm preservation", "[gates]")
{
    std::mt19937_64 rng(123);

    qert::Statevector sv(4);
    qert::apply_hadamard(sv.data(), sv.num_qubits(), 0);
    qert::apply_hadamard(sv.data(), sv.num_qubits(), 1);

    double norm_before = sv.norm();

    auto mat = qert::generate_random_su4(rng);
    qert::apply_two_qubit_unitary(sv.data(), sv.num_qubits(), 0, 1, mat.data());

    REQUIRE(std::abs(sv.norm() - norm_before) < 1e-12);
}

TEST_CASE("Arbitrary single-qubit unitary", "[gates]")
{
    qert::Statevector sv(1);

    // Apply identity
    qert::apply_single_qubit_unitary(sv.data(), sv.num_qubits(), 0,
                                     {1, 0}, {0, 0}, {0, 0}, {1, 0});
    REQUIRE(std::abs(sv.amplitude(0) - qert::Complex{1.0, 0.0}) < 1e-12);

    // Apply X via unitary
    qert::apply_single_qubit_unitary(sv.data(), sv.num_qubits(), 0,
                                     {0, 0}, {1, 0}, {1, 0}, {0, 0});
    REQUIRE(std::abs(sv.amplitude(1) - qert::Complex{1.0, 0.0}) < 1e-12);
}
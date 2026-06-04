#include "qert/entropy.hpp"
#include "qert/gates.hpp"
#include "qert/statevector.hpp"

#include <catch_amalgamated.hpp>

TEST_CASE("Zero entropy for |0...0⟩", "[entropy]")
{
    qert::Statevector sv(4);
    double s = qert::compute_half_chain_entropy(sv.data(), sv.num_qubits());
    REQUIRE(std::abs(s) < 1e-10);
}

TEST_CASE("Bell state entropy = ln(2)", "[entropy]")
{
    qert::Statevector sv(2);
    qert::apply_hadamard(sv.data(), sv.num_qubits(), 0);
    qert::apply_cnot(sv.data(), sv.num_qubits(), 0, 1);

    double s = qert::compute_half_chain_entropy(sv.data(), sv.num_qubits());
    REQUIRE(std::abs(s - std::log(2.0)) < 1e-6);
}

TEST_CASE("Product state has zero entropy", "[entropy]")
{
    qert::Statevector sv(2);
    qert::apply_hadamard(sv.data(), sv.num_qubits(), 0);
    qert::apply_hadamard(sv.data(), sv.num_qubits(), 1);

    double s = qert::compute_half_chain_entropy(sv.data(), sv.num_qubits());
    REQUIRE(std::abs(s) < 1e-10);
}

TEST_CASE("GHZ state entropy = ln(2)", "[entropy]")
{
    qert::Statevector sv(3);
    qert::apply_hadamard(sv.data(), sv.num_qubits(), 0);
    qert::apply_cnot(sv.data(), sv.num_qubits(), 0, 1);
    qert::apply_cnot(sv.data(), sv.num_qubits(), 0, 2);

    double s = qert::compute_half_chain_entropy(sv.data(), sv.num_qubits());
    REQUIRE(std::abs(s - std::log(2.0)) < 1e-6);
}

TEST_CASE("Entropy grows with random circuit depth", "[entropy]")
{
    std::mt19937_64 rng(42);
    qert::Statevector sv(6);

    double prev = 0.0;
    bool grew = false;

    for (int layer = 0; layer < 20; ++layer)
    {
        if (layer % 2 == 0)
        {
            for (uint32_t q = 0; q < 5; q += 2)
            {
                auto mat = qert::generate_random_su4(rng);
                qert::apply_two_qubit_unitary(sv.data(), sv.num_qubits(), q, q + 1, mat.data());
            }
        }
        else
        {
            for (uint32_t q = 1; q < 5; q += 2)
            {
                auto mat = qert::generate_random_su4(rng);
                qert::apply_two_qubit_unitary(sv.data(), sv.num_qubits(), q, q + 1, mat.data());
            }
        }

        double s = qert::compute_half_chain_entropy(sv.data(), sv.num_qubits());
        if (s > prev + 1e-10)
            grew = true;
        prev = s;
    }

    REQUIRE(grew);
}

TEST_CASE("Entropy within theoretical bound", "[entropy]")
{
    qert::Statevector sv(6);
    std::mt19937_64 rng(99);

    for (int layer = 0; layer < 50; ++layer)
    {
        for (uint32_t q = 0; q < 5; q += 2)
        {
            auto mat = qert::generate_random_su4(rng);
            qert::apply_two_qubit_unitary(sv.data(), sv.num_qubits(), q, q + 1, mat.data());
        }
    }

    double s = qert::compute_half_chain_entropy(sv.data(), sv.num_qubits());
    double max_entropy = 3.0 * std::log(2.0); // k=3, max = 3*ln(2)
    REQUIRE(s <= max_entropy + 1e-10);
}

TEST_CASE("Explicit bipartition k parameter", "[entropy]")
{
    qert::Statevector sv(4);
    qert::apply_hadamard(sv.data(), sv.num_qubits(), 0);
    qert::apply_cnot(sv.data(), sv.num_qubits(), 0, 1);

    // Bell pair on qubits 0,1. k=1 splits A={0}, B={1,2,3}
    double s = qert::compute_entropy(sv.data(), sv.num_qubits(), 1);
    REQUIRE(std::abs(s - std::log(2.0)) < 1e-6);
}
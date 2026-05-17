#include <catch_amalgamated.hpp>
#include "statevector.hpp"
#include "common.hpp"

TEST_CASE("Statevector construction", "[statevector]")
{
    qert::Statevector sv(4);
    REQUIRE(sv.num_qubits() == 4);
    REQUIRE(sv.size() == 16);
    REQUIRE(std::abs(sv.norm() - 1.0) < 1e-12);
}

TEST_CASE("Statevector initial state is |0...0⟩", "[statevector]")
{
    qert::Statevector sv(3);

    REQUIRE(std::abs(sv.amplitude(0) - qert::Complex{1.0, 0.0}) < 1e-12);

    for (qert::StateIndex i = 1; i < sv.size(); ++i)
    {
        REQUIRE(std::abs(sv.amplitude(i)) < 1e-12);
    }
}

TEST_CASE("Statevector reset_to_zero", "[statevector]")
{
    qert::Statevector sv(3);
    sv.amplitude(5) = qert::Complex{0.7, 0.3};
    sv.reset_to_zero();

    REQUIRE(std::abs(sv.amplitude(0) - qert::Complex{1.0, 0.0}) < 1e-12);
    REQUIRE(std::abs(sv.amplitude(5)) < 1e-12);
}

TEST_CASE("Statevector validation", "[statevector]")
{
    REQUIRE_THROWS(qert::Statevector(0));
    REQUIRE_THROWS(qert::Statevector(qert::MAX_QUBITS + 1));
}

TEST_CASE("Statevector move semantics", "[statevector]")
{
    qert::Statevector sv1(4);
    qert::Statevector sv2(std::move(sv1));
    REQUIRE(sv2.num_qubits() == 4);
    REQUIRE(sv2.size() == 16);
}
#include "qert/common.hpp"

#include <catch_amalgamated.hpp>

TEST_CASE("Common types have correct sizes", "[common]")
{
    REQUIRE(sizeof(qert::Complex) == 16);
    REQUIRE(sizeof(qert::StateIndex) == 8);
    REQUIRE(sizeof(qert::Qubit) == 4);
    REQUIRE(sizeof(qert::GateIndex) == 4);
    REQUIRE(sizeof(qert::Depth) == 4);
    REQUIRE(sizeof(qert::EventID) == 8);
}

TEST_CASE("MAX_QUBITS is reasonable", "[common]")
{
    REQUIRE(qert::MAX_QUBITS == 32);
    // 2^32 * 16 bytes = 64 GB (feasibility limit)
    REQUIRE((1ULL << qert::MAX_QUBITS) > 0);
}
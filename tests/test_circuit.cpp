#include "qert/circuit.hpp"

#include <catch_amalgamated.hpp>

TEST_CASE("Brickwall circuit structure N=4", "[circuit]")
{
    auto circ = qert::Circuit::brickwall_1d(4, 4);

    REQUIRE(circ.num_qubits() == 4);
    REQUIRE(circ.depth() == 4);

    // Layer 0: (0,1), (2,3) = 2 gates
    // Layer 1: (1,2) = 1 gate
    // Layer 2: (0,1), (2,3) = 2 gates
    // Layer 3: (1,2) = 1 gate
    REQUIRE(circ.num_gates() == 6);
    REQUIRE(circ.layer_size(0) == 2);
    REQUIRE(circ.layer_size(1) == 1);
    REQUIRE(circ.layer_size(2) == 2);
    REQUIRE(circ.layer_size(3) == 1);
}

TEST_CASE("Brickwall circuit structure N=6", "[circuit]")
{
    auto circ = qert::Circuit::brickwall_1d(6, 4);

    REQUIRE(circ.layer_size(0) == 3); // (0,1), (2,3), (4,5)
    REQUIRE(circ.layer_size(1) == 2); // (1,2), (3,4)
}

TEST_CASE("Brickwall all gates are RANDOM_SU4", "[circuit]")
{
    auto circ = qert::Circuit::brickwall_1d(8, 10);

    for (uint32_t i = 0; i < circ.num_gates(); ++i)
    {
        REQUIRE(circ.gate(i).type == qert::GateType::RANDOM_SU4);
    }
}

TEST_CASE("Brickwall no overlapping qubits per layer", "[circuit]")
{
    for (uint32_t n = 4; n <= 12; n += 2)
    {
        auto circ = qert::Circuit::brickwall_1d(n, 6);

        for (uint32_t d = 0; d < circ.depth(); ++d)
        {
            std::vector<bool> used(n, false);

            for (uint32_t i = 0; i < circ.num_gates(); ++i)
            {
                const auto &g = circ.gate(i);
                if (g.depth != d)
                    continue;

                REQUIRE(!used[g.control]);
                REQUIRE(!used[g.target]);
                used[g.control] = true;
                used[g.target] = true;
            }
        }
    }
}

TEST_CASE("Brickwall odd N", "[circuit]")
{
    auto circ = qert::Circuit::brickwall_1d(5, 3);
    // Layer 0: (0,1), (2,3) — qubit 4 idle
    // Layer 1: (1,2), (3,4)
    // Layer 2: (0,1), (2,3)
    REQUIRE(circ.num_gates() == 6);
    REQUIRE(circ.layer_size(0) == 2);
    REQUIRE(circ.layer_size(1) == 2);
    REQUIRE(circ.layer_size(2) == 2);
}

TEST_CASE("Circuit validation", "[circuit]")
{
    REQUIRE_THROWS(qert::Circuit(0, 4));
    REQUIRE_THROWS(qert::Circuit(4, 0));
    REQUIRE_THROWS(qert::Circuit(33, 4)); // > MAX_QUBITS
}
#include <catch_amalgamated.hpp>
#include "circuit.hpp"
#include "gates.hpp"
#include "statevector.hpp"
#include "telemetry.hpp"
#include "entropy.hpp"
#include <random>
#include <fstream>
#include <cmath>

TEST_CASE("Full circuit execution + telemetry", "[integration]")
{
    auto circ = qert::Circuit::brickwall_1d(8, 10);
    qert::Statevector sv(8);
    std::mt19937_64 rng(42);

    qert::TelemetryRecorder rec("integration_test.csv", "{}");

    qert::EventID event_id = 0;

    for (uint32_t i = 0; i < circ.num_gates(); ++i)
    {
        const auto &g = circ.gate(i);

        qert::TelemetryEvent ev;
        ev.event_id = event_id++;
        ev.depth = g.depth;
        ev.gate_idx = g.gate_idx;
        ev.execution_time_ns = 0;
        ev.l3_misses_delta = 0;
        ev.tlb_misses_delta = 0;
        ev.working_set_kb = 0;
        ev.stride_entropy = 0.0;

        bool is_last = (g.gate_idx == circ.layer_size(g.depth) - 1);
        bool is_even = (g.depth % 2 == 0);

        if (is_last && is_even)
        {
            ev.half_chain_entropy = qert::compute_half_chain_entropy(
                sv.data(), sv.num_qubits());
        }
        else
        {
            ev.half_chain_entropy = std::nan("");
        }

        auto mat = qert::generate_random_su4(rng);
        qert::apply_two_qubit_unitary(sv.data(), sv.num_qubits(),
                                      g.control, g.target, mat.data());

        rec.record(ev);
    }

    rec.close();
    REQUIRE(rec.event_count() == circ.num_gates());

    // Verify file exists and has correct structure
    std::ifstream f("integration_test.csv");
    REQUIRE(f.good());

    std::string line;
    std::getline(f, line);
    REQUIRE(line.find("# {") == 0); // Metadata

    std::getline(f, line);
    REQUIRE(line.find("event_id") != std::string::npos); // Header

    int data_rows = 0;
    while (std::getline(f, line))
    {
        if (!line.empty())
            data_rows++;
    }
    REQUIRE(data_rows == static_cast<int>(circ.num_gates()));
}

TEST_CASE("Norm preserved through brickwall circuit", "[integration]")
{
    auto circ = qert::Circuit::brickwall_1d(10, 20);
    qert::Statevector sv(10);
    std::mt19937_64 rng(123);

    double initial_norm = sv.norm();

    for (uint32_t i = 0; i < circ.num_gates(); ++i)
    {
        const auto &g = circ.gate(i);
        auto mat = qert::generate_random_su4(rng);
        qert::apply_two_qubit_unitary(sv.data(), sv.num_qubits(),
                                      g.control, g.target, mat.data());
    }

    REQUIRE(std::abs(sv.norm() - initial_norm) < 1e-10);
}

TEST_CASE("Entropy grows and saturates in brickwall", "[integration]")
{
    auto circ = qert::Circuit::brickwall_1d(10, 40);
    qert::Statevector sv(10);
    std::mt19937_64 rng(456);

    double prev_entropy = 0.0;
    int growth_count = 0;
    int plateau_count = 0;

    for (uint32_t i = 0; i < circ.num_gates(); ++i)
    {
        const auto &g = circ.gate(i);

        auto mat = qert::generate_random_su4(rng);
        qert::apply_two_qubit_unitary(sv.data(), sv.num_qubits(),
                                      g.control, g.target, mat.data());

        bool is_last = (g.gate_idx == circ.layer_size(g.depth) - 1);
        bool is_even = (g.depth % 2 == 0);

        if (is_last && is_even)
        {
            double s = qert::compute_half_chain_entropy(sv.data(), sv.num_qubits());

            if (s > prev_entropy + 0.01)
            {
                growth_count++;
            }
            else if (std::abs(s - prev_entropy) < 0.02)
            {
                if (prev_entropy > 1.0)
                { // Only count plateau after significant growth
                    plateau_count++;
                }
            }

            prev_entropy = s;
        }
    }

    // Should see growth phase followed by saturation
    REQUIRE(growth_count > 0);
    REQUIRE(plateau_count > 0);
}
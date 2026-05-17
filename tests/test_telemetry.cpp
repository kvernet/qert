#include <catch_amalgamated.hpp>
#include "telemetry.hpp"
#include <fstream>
#include <cmath>

TEST_CASE("TelemetryRecorder creates file with metadata", "[telemetry]")
{
    std::string meta = "{\"test\": true}";
    qert::TelemetryRecorder rec("test_tel.csv", meta);
    rec.close();

    std::ifstream f("test_tel.csv");
    REQUIRE(f.good());

    std::string line;
    std::getline(f, line);
    REQUIRE(line.find("# {") == 0);
}

TEST_CASE("TelemetryRecorder writes CSV header", "[telemetry]")
{
    qert::TelemetryRecorder rec("test_tel2.csv", "{}");
    rec.close();

    std::ifstream f("test_tel2.csv");
    std::string line;
    std::getline(f, line); // Skip metadata
    std::getline(f, line); // Header

    REQUIRE(line.find("event_id") != std::string::npos);
    REQUIRE(line.find("depth") != std::string::npos);
    REQUIRE(line.find("half_chain_entropy") != std::string::npos);
}

TEST_CASE("TelemetryRecorder records events", "[telemetry]")
{
    qert::TelemetryRecorder rec("test_tel3.csv", "{}");

    qert::TelemetryEvent ev;
    ev.event_id = 0;
    ev.depth = 1;
    ev.gate_idx = 2;
    ev.execution_time_ns = 1000;
    ev.l3_misses_delta = 10;
    ev.tlb_misses_delta = 2;
    ev.working_set_kb = 64;
    ev.stride_entropy = 3.5;
    ev.half_chain_entropy = std::nan("");

    rec.record(ev);
    REQUIRE(rec.event_count() == 1);

    ev.event_id = 1;
    ev.half_chain_entropy = 0.693147;
    rec.record(ev);
    REQUIRE(rec.event_count() == 2);

    rec.close();

    std::ifstream f("test_tel3.csv");
    std::string line;
    std::getline(f, line); // Metadata
    std::getline(f, line); // Header
    std::getline(f, line); // Row 1
    REQUIRE(line.find("nan") != std::string::npos);
    std::getline(f, line); // Row 2
    REQUIRE(line.find("0.693147") != std::string::npos);
}

TEST_CASE("TelemetryRecorder auto-closes on destructor", "[telemetry]")
{
    {
        qert::TelemetryRecorder rec("test_tel4.csv", "{}");
        qert::TelemetryEvent ev{};
        ev.half_chain_entropy = 0.0;
        rec.record(ev);
    }

    std::ifstream f("test_tel4.csv");
    REQUIRE(f.good());
}
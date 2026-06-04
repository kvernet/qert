#include "qert/seed.hpp"

#include <build_info.hpp>
#include <catch_amalgamated.hpp>

#include <fstream>

TEST_CASE("Global RNG seed", "[seed]")
{
    REQUIRE_THROWS(qert::get_rng_seed()); // Not set yet

    qert::set_rng_seed(42);
    REQUIRE(qert::get_rng_seed() == 42);

    REQUIRE_THROWS(qert::set_rng_seed(99)); // Already set

    // Note: can't reset global seed between tests.
    // This is acceptable for Phase 1 single-threaded use.
}

TEST_CASE("RunMetadata construction", "[seed]")
{
    qert::RunMetadata meta("brickwall_1d", 16, 48, "lexicographic", 42, "abc1234", "gcc-13", "-O2",
                           "x86_64", "test-cpu", 0);

    REQUIRE(meta.circuit_family == "brickwall_1d");
    REQUIRE(meta.num_qubits == 16);
    REQUIRE(meta.circuit_depth_total == 48);
    REQUIRE(meta.qubit_mapping == "lexicographic");
    REQUIRE(meta.rng_seed == 42);
    REQUIRE(meta.git_commit == "abc1234");
    REQUIRE(meta.compiler == "gcc-13");
    REQUIRE(meta.compiler_flags == "-O2");
    REQUIRE(meta.cpu_arch == "x86_64");
    REQUIRE(meta.cpu_model == "test-cpu");
    REQUIRE(meta.timestamp_unix_ns == 0);
}

TEST_CASE("RunMetadata validation", "[seed]")
{
    // Empty circuit_family
    REQUIRE_THROWS(qert::RunMetadata("", 16, 48, "lex", 42, "a", "b", "c", "d", "e", 0));

    // num_qubits == 0
    REQUIRE_THROWS(qert::RunMetadata("brick", 0, 48, "lex", 42, "a", "b", "c", "d", "e", 0));

    // num_qubits > MAX_QUBITS
    REQUIRE_THROWS(qert::RunMetadata("brick", 33, 48, "lex", 42, "a", "b", "c", "d", "e", 0));

    // circuit_depth_total == 0
    REQUIRE_THROWS(qert::RunMetadata("brick", 16, 0, "lex", 42, "a", "b", "c", "d", "e", 0));

    // Empty qubit_mapping
    REQUIRE_THROWS(qert::RunMetadata("brick", 16, 48, "", 42, "a", "b", "c", "d", "e", 0));

    // rng_seed == 0
    REQUIRE_THROWS(qert::RunMetadata("brick", 16, 48, "lex", 0, "a", "b", "c", "d", "e", 0));
}

TEST_CASE("Physics hash stability", "[seed]")
{
    qert::RunMetadata meta1("brickwall_1d", 16, 48, "lexicographic", 42, "abc", "gcc", "-O2",
                            "x86_64", "cpu", 0);
    qert::RunMetadata meta2("brickwall_1d", 16, 48, "lexicographic", 42, "def", "clang", "-O3",
                            "arm", "other-cpu", 999);

    // Same physics → same physics_hash regardless of environment
    REQUIRE(meta1.physics_hash == meta2.physics_hash);

    // Different environment → different environment_hash
    REQUIRE(meta1.environment_hash != meta2.environment_hash);

    // Different timestamps → different full_hash
    REQUIRE(meta1.full_hash != meta2.full_hash);
}

TEST_CASE("RunMetadata JSON serialization", "[seed]")
{
    qert::RunMetadata meta("brickwall_1d", 16, 48, "lexicographic", 42, "abc", "gcc", "-O2",
                           "x86_64", "cpu", 0);

    std::string json = meta.to_json();

    REQUIRE(json.find("\"circuit_family\": \"brickwall_1d\"") != std::string::npos);
    REQUIRE(json.find("\"num_qubits\": 16") != std::string::npos);
    REQUIRE(json.find("\"physics_hash\":") != std::string::npos);
    REQUIRE(json.find("\"environment_hash\":") != std::string::npos);
    REQUIRE(json.find("\"full_hash\":") != std::string::npos);
}

TEST_CASE("RunMetadata file I/O", "[seed]")
{
    qert::RunMetadata meta("test", 4, 10, "gray", 12345, "abc", "gcc", "-O2", "x86_64", "cpu", 0);

    meta.write_to_file("test_meta.json");

    std::ifstream f("test_meta.json");
    REQUIRE(f.good());

    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    REQUIRE(content.find("\"circuit_family\": \"test\"") != std::string::npos);
}
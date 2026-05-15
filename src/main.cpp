// src/main.cpp (temporary: test compilation + seed infrastructure)
#include "common.hpp"
#include "seed.hpp"
#include "statevector.hpp"
#include <build_info.hpp>

#include <iostream>

int random_seed();
int state_vector();

int main()
{
    if (random_seed() || state_vector()) {
        return 1;
    }

    std::printf("All tests passed.\n");

    return 0;
}

int random_seed() {
    // Test global seed
    qert::set_rng_seed(42);
    if (qert::get_rng_seed() != 42)
    {
        std::fprintf(stderr, "FAIL: seed roundtrip\n");
        return 1;
    }

    // Test metadata construction
    qert::RunMetadata meta(
        "brickwall_1d",
        16,
        48,
        "lexicographic",
        42,
        qert::GIT_COMMIT,
        std::string(qert::COMPILER_ID) + "-" + qert::COMPILER_VERSION,
        "-O2",
        "x86_64",
        "test-cpu",
        0);

    // Print JSON to stdout for inspection
    std::string json = meta.to_json();
    std::printf("%s\n", json.c_str());

    // Test write to file
    meta.write_to_file("test_metadata.json");
    std::printf("Wrote test_metadata.json\n");

    // Test that physics_hash is stable
    qert::RunMetadata meta2(
        "brickwall_1d",
        16,
        48,
        "lexicographic",
        42,
        qert::GIT_COMMIT,
        std::string(qert::COMPILER_ID) + "-" + qert::COMPILER_VERSION,
        "-O2",
        "x86_64",
        "test-cpu",
        1 // different timestamp
    );

    if (meta.physics_hash != meta2.physics_hash)
    {
        std::fprintf(stderr, "FAIL: physics_hash not stable across timestamps\n");
        return 1;
    }

    if (meta.full_hash == meta2.full_hash)
    {
        std::fprintf(stderr, "FAIL: full_hash should differ with different timestamps\n");
        return 1;
    }

    return 0;
}

int state_vector()
{
    // Test statevector construction
    qert::Statevector sv(4);
    if (sv.num_qubits() != 4)
    {
        std::fprintf(stderr, "FAIL: num_qubits\n");
        return 1;
    }
    if (sv.size() != 16)
    {
        std::fprintf(stderr, "FAIL: size\n");
        return 1;
    }

    // Test initial state |0000⟩
    if (std::abs(sv.amplitude(0) - qert::Complex{1.0, 0.0}) > qert::NUMERICAL_EPSILON)
    {
        std::fprintf(stderr, "FAIL: initial state |0⟩\n");
        return 1;
    }
    if (std::abs(sv.amplitude(1)) > qert::NUMERICAL_EPSILON)
    {
        std::fprintf(stderr, "FAIL: non-zero amplitude at index 1\n");
        return 1;
    }

    // Test norm
    if (std::abs(sv.norm() - 1.0) > qert::NUMERICAL_EPSILON)
    {
        std::fprintf(stderr, "FAIL: norm\n");
        return 1;
    }

    // Test reset
    sv.amplitude(3) = qert::Complex{0.5, 0.0};
    sv.reset_to_zero();
    if (std::abs(sv.amplitude(3)) > qert::NUMERICAL_EPSILON)
    {
        std::fprintf(stderr, "FAIL: reset_to_zero\n");
        return 1;
    }

    // Test move semantics compile
    qert::Statevector sv2(std::move(sv));
    if (sv2.num_qubits() != 4)
    {
        std::fprintf(stderr, "FAIL: move construction\n");
        return 1;
    }

    return 0;
}
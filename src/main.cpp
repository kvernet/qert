// src/main.cpp (temporary: test compilation + seed infrastructure)
#include "common.hpp"
#include "seed.hpp"
#include <build_info.hpp>

#include <cstdio>

int main()
{
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

    std::printf("All tests passed.\n");
    return 0;
}
#pragma once

#include <cstdint>
#include <string>

namespace qert
{

struct RunMetadata
{
    // --- Circuit identity (the physics experiment) ---
    const std::string circuit_family;
    const uint32_t num_qubits;
    const uint32_t circuit_depth_total;
    const std::string qubit_mapping;
    const uint64_t rng_seed;

    // --- Build identity (how the binary was produced) ---
    const std::string git_commit;
    const std::string compiler;
    const std::string compiler_flags;

    // --- Platform identity (where it ran) ---
    const std::string cpu_arch;
    const std::string cpu_model;

    // --- Run identity ---
    const uint64_t timestamp_unix_ns;

    // --- Experiment fingerprints ---
    // physics_hash:    Identifies the scientific condition.
    //                  Same circuit + seed + mapping = same physics_hash
    //                  regardless of compiler, platform, or build.
    //                  Use for grouping runs that test the same hypothesis.
    //
    // environment_hash: Identifies the execution environment.
    //                   Same compiler + flags + platform = same environment_hash.
    //                   Use for detecting platform-specific effects.
    //
    // full_hash:        Unique identifier for this exact run.
    //                   Combines physics_hash, environment_hash, and timestamp.
    //                   Use for deduplication and provenance tracking.
    uint64_t physics_hash;
    uint64_t environment_hash;
    uint64_t full_hash;

    // --- Construction ---
    // All fields required. No default constructor.
    // Hashes are computed automatically during construction.
    RunMetadata(std::string circuit_family, uint32_t num_qubits, uint32_t circuit_depth_total,
                std::string qubit_mapping, uint64_t rng_seed, std::string git_commit,
                std::string compiler, std::string compiler_flags, std::string cpu_arch,
                std::string cpu_model, uint64_t timestamp_unix_ns);

    // --- Serialization ---
    std::string to_json() const;
    void write_to_file(const std::string &path) const;

  private:
    uint64_t compute_physics_hash() const;
    uint64_t compute_environment_hash() const;
};

// --- Global seed state ---
// Set once at program startup. Read by metadata construction.
// Phase 1: single-threaded, no runtime stochasticity.
// If Phase 2 introduces runtime RNG, refactor to explicit injection.

void set_rng_seed(uint64_t seed);
uint64_t get_rng_seed();

} // namespace qert
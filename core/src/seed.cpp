#include "qert/seed.hpp"
#include "qert/common.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <stdexcept>

namespace qert
{

// --- Global seed state ---
namespace
{
uint64_t g_rng_seed = 0;
bool g_seed_set = false;
} // namespace

void set_rng_seed(uint64_t seed)
{
    if (g_seed_set)
    {
        throw std::logic_error("RNG seed already set. Cannot set twice.");
    }
    if (seed == 0)
    {
        throw std::invalid_argument("RNG seed must be non-zero.");
    }
    g_rng_seed = seed;
    g_seed_set = true;
}

uint64_t get_rng_seed()
{
    if (!g_seed_set)
    {
        throw std::logic_error("RNG seed not set. Call set_rng_seed() first.");
    }
    return g_rng_seed;
}

// --- Deterministic hashing (FNV-1a 64-bit) ---
// std::hash is intentionally unspecified across implementations.
// FNV-1a is deterministic, platform-independent, and trivial to implement.

namespace
{
constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
constexpr uint64_t FNV_PRIME = 1099511628211ULL;

uint64_t fnv1a_64(const void *data, size_t len, uint64_t hash = FNV_OFFSET)
{
    const auto *bytes = static_cast<const uint8_t *>(data);
    for (size_t i = 0; i < len; ++i)
    {
        hash ^= bytes[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

uint64_t fnv1a_64_str(const std::string &s)
{
    return fnv1a_64(s.data(), s.size());
}

// Combine two hashes in an order-dependent way.
uint64_t hash_combine(uint64_t a, uint64_t b)
{
    return a ^ (b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2));
}
} // namespace

// --- RunMetadata ---

uint64_t RunMetadata::compute_physics_hash() const
{
    uint64_t h = FNV_OFFSET;
    h = fnv1a_64_str(circuit_family);
    h = hash_combine(h, num_qubits);
    h = hash_combine(h, circuit_depth_total);
    h = hash_combine(h, fnv1a_64_str(qubit_mapping));
    h = hash_combine(h, rng_seed);
    return h;
}

uint64_t RunMetadata::compute_environment_hash() const
{
    uint64_t e = FNV_OFFSET;
    e = hash_combine(e, fnv1a_64_str(git_commit));
    e = hash_combine(e, fnv1a_64_str(compiler));
    e = hash_combine(e, fnv1a_64_str(compiler_flags));
    e = hash_combine(e, fnv1a_64_str(cpu_arch));
    e = hash_combine(e, fnv1a_64_str(cpu_model));
    return e;
}

RunMetadata::RunMetadata(std::string circuit_family, uint32_t num_qubits,
                         uint32_t circuit_depth_total, std::string qubit_mapping, uint64_t rng_seed,
                         std::string git_commit, std::string compiler, std::string compiler_flags,
                         std::string cpu_arch, std::string cpu_model, uint64_t timestamp_unix_ns)
    : circuit_family(std::move(circuit_family)), num_qubits(num_qubits),
      circuit_depth_total(circuit_depth_total), qubit_mapping(std::move(qubit_mapping)),
      rng_seed(rng_seed), git_commit(std::move(git_commit)), compiler(std::move(compiler)),
      compiler_flags(std::move(compiler_flags)), cpu_arch(std::move(cpu_arch)),
      cpu_model(std::move(cpu_model)), timestamp_unix_ns(timestamp_unix_ns), physics_hash(0),
      environment_hash(0), full_hash(0)
{
    // --- Validation ---
    if (num_qubits == 0 || num_qubits > MAX_QUBITS)
    {
        throw std::invalid_argument("num_qubits must be in [1, " + std::to_string(MAX_QUBITS) +
                                    "]. "
                                    "Upper bound is a memory feasibility constraint.");
    }
    if (circuit_depth_total == 0)
    {
        throw std::invalid_argument("circuit_depth_total must be > 0");
    }
    if (this->circuit_family.empty())
    {
        throw std::invalid_argument("circuit_family must not be empty");
    }
    if (this->qubit_mapping.empty())
    {
        throw std::invalid_argument("qubit_mapping must not be empty");
    }
    if (this->rng_seed == 0)
    {
        throw std::invalid_argument("rng_seed must be non-zero");
    }

    // --- Compute hashes ---
    // Physics hash: what experiment was run (circuit + seed + mapping).
    // Stable across compilers, platforms, and build configurations.
    physics_hash = compute_physics_hash();

    // Environment hash: how the experiment was executed.
    environment_hash = compute_environment_hash();

    // Full hash: unique identifier for this exact run configuration.
    // Combines physics identity, environment identity, and timestamp.
    full_hash = hash_combine(physics_hash, environment_hash);
    full_hash = hash_combine(full_hash, timestamp_unix_ns);
}

std::string RunMetadata::to_json() const
{
    auto ns = timestamp_unix_ns;
    auto seconds = static_cast<std::time_t>(ns / 1'000'000'000);
    auto nanoseconds = ns % 1'000'000'000;

    // Thread-safe timestamp formatting.
    struct tm utc_tm;
#ifdef _WIN32
    gmtime_s(&utc_tm, &seconds);
#else
    gmtime_r(&seconds, &utc_tm);
#endif

    char iso_buf[64];
    std::strftime(iso_buf, sizeof(iso_buf), "%Y-%m-%dT%H:%M:%S", &utc_tm);

    std::string json;
    json.reserve(512);

    json += "{\n";
    json += "  \"circuit_family\": \"" + circuit_family + "\",\n";
    json += "  \"num_qubits\": " + std::to_string(num_qubits) + ",\n";
    json += "  \"circuit_depth_total\": " + std::to_string(circuit_depth_total) + ",\n";
    json += "  \"qubit_mapping\": \"" + qubit_mapping + "\",\n";
    json += "  \"rng_seed\": " + std::to_string(rng_seed) + ",\n";
    json += "  \"git_commit\": \"" + git_commit + "\",\n";
    json += "  \"compiler\": \"" + compiler + "\",\n";
    json += "  \"compiler_flags\": \"" + compiler_flags + "\",\n";
    json += "  \"cpu_arch\": \"" + cpu_arch + "\",\n";
    json += "  \"cpu_model\": \"" + cpu_model + "\",\n";
    json += "  \"timestamp_unix_ns\": " + std::to_string(timestamp_unix_ns) + ",\n";
    json += "  \"timestamp_utc\": \"" + std::string(iso_buf) + "." + std::to_string(nanoseconds) +
            "Z\",\n";
    json += "  \"physics_hash\": " + std::to_string(physics_hash) + ",\n";
    json += "  \"environment_hash\": " + std::to_string(environment_hash) + ",\n";
    json += "  \"full_hash\": " + std::to_string(full_hash) + "\n";
    json += "}\n";

    return json;
}

void RunMetadata::write_to_file(const std::string &path) const
{
    std::ofstream file(path);
    if (!file)
    {
        throw std::runtime_error("Failed to open metadata file: " + path);
    }
    file << to_json();
    if (!file)
    {
        throw std::runtime_error("Failed to write metadata to: " + path);
    }
}

} // namespace qert
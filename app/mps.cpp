//
// MPS simulator.
// Runs a brickwall circuit through an MPS simulator, recording:
//   - half-chain entanglement entropy (via statevector)
//   - max bond dimension χ at each sampled layer
//   - truncation error
//
// The CLI interface matches the statevector qert binary so that
// the same sweep scripts (single.sh) can drive both simulators.
//
// Usage:
//   ./build/qert_mps --num-qubits 16 --depth 48 --chi-max 64 \
//                    --seed 42 --mapping lexicographic \
//                    --output results/run.csv

#include "qert/mps.hpp"
#include "qert/circuit.hpp"
#include "qert/common.hpp"
#include "qert/entropy.hpp"
#include "qert/gates.hpp"
#include "qert/seed.hpp"
#include "qert/statevector.hpp"
#include "qert/telemetry.hpp"
#include <build_info.hpp>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace
{

struct CliArgs
{
    uint32_t num_qubits = 0;
    uint32_t depth = 0;
    uint32_t chi_max = 64;
    uint64_t seed = 0;
    std::string mapping;
    std::string output_path;
    std::string circuit_family = "brickwall_1d";
    bool valid = false;
};

CliArgs parse_args(int argc, char **argv)
{
    CliArgs args;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "--num-qubits" && i + 1 < argc)
            args.num_qubits = static_cast<uint32_t>(std::atoi(argv[++i]));
        else if (arg == "--depth" && i + 1 < argc)
            args.depth = static_cast<uint32_t>(std::atoi(argv[++i]));
        else if (arg == "--chi-max" && i + 1 < argc)
            args.chi_max = static_cast<uint32_t>(std::atoi(argv[++i]));
        else if (arg == "--seed" && i + 1 < argc)
            args.seed = static_cast<uint64_t>(std::atoll(argv[++i]));
        else if (arg == "--mapping" && i + 1 < argc)
            args.mapping = argv[++i];
        else if (arg == "--output" && i + 1 < argc)
            args.output_path = argv[++i];
        else if (arg == "--circuit-family" && i + 1 < argc)
            args.circuit_family = argv[++i];
        else if (arg == "--help" || arg == "-h")
        {
            std::printf("qert_mps — MPS bond dimension simulator\n\n"
                        "Usage:\n"
                        "  qert_mps --num-qubits N --depth D --seed S "
                        "--mapping M --output PATH [--chi-max C]\n\n"
                        "Required arguments:\n"
                        "  --num-qubits N    Number of qubits (4-%u)\n"
                        "  --depth D         Circuit depth (number of layers)\n"
                        "  --seed S          RNG seed (non-zero)\n"
                        "  --mapping M       Qubit mapping: lexicographic, gray, "
                        "locality_aware\n"
                        "  --output PATH     Output CSV file path\n\n"
                        "Optional arguments:\n"
                        "  --chi-max C       MPS max bond dimension (default: 64)\n"
                        "  --circuit-family F  Circuit family (default: brickwall_1d)\n"
                        "  --help, -h          Show this message\n",
                        qert::MAX_QUBITS);
            return args;
        }
        else
        {
            std::fprintf(stderr, "Unknown argument: %s\n", arg.c_str());
            return args;
        }
    }

    if (args.num_qubits == 0 || args.depth == 0 || args.seed == 0 || args.mapping.empty() ||
        args.output_path.empty())
    {
        std::fprintf(stderr, "Error: missing required arguments. Use --help for usage.\n");
        return args;
    }

    if (args.num_qubits > qert::MAX_QUBITS)
    {
        std::fprintf(stderr, "Error: num_qubits %u exceeds MAX_QUBITS %u\n", args.num_qubits,
                     qert::MAX_QUBITS);
        return args;
    }

    args.valid = true;
    return args;
}

// --- CPU Detection (same as app/main.cpp) ---

std::string detect_cpu_model()
{
#ifdef __linux__
    FILE *f = std::fopen("/proc/cpuinfo", "r");
    if (!f)
        return "unknown";

    char line[256];
    while (std::fgets(line, sizeof(line), f))
    {
        if (std::strncmp(line, "model name", 10) == 0)
        {
            const char *colon = std::strchr(line, ':');
            if (colon)
            {
                std::fclose(f);
                std::string model = colon + 2;
                while (!model.empty() && (model.back() == '\n' || model.back() == '\r'))
                    model.pop_back();
                return model;
            }
        }
    }
    std::fclose(f);
    return "unknown";
#elif defined(__APPLE__)
    FILE *f = ::popen("sysctl -n machdep.cpu.brand_string", "r");
    if (!f)
        return "unknown";

    char buf[256];
    if (std::fgets(buf, sizeof(buf), f))
    {
        ::pclose(f);
        std::string model = buf;
        while (!model.empty() && (model.back() == '\n' || model.back() == '\r'))
            model.pop_back();
        return model;
    }
    ::pclose(f);
    return "unknown";
#else
    return "unknown";
#endif
}

std::string detect_cpu_arch()
{
#ifdef __x86_64__
    return "x86_64";
#elif defined(__aarch64__)
    return "aarch64";
#else
    return "unknown";
#endif
}

uint64_t get_current_unix_ns()
{
    auto now = std::chrono::system_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());
}

} // anonymous namespace

int main(int argc, char **argv)
{
    CliArgs args = parse_args(argc, argv);
    if (!args.valid)
        return 1;

    // --- Initialize RNG ---
    qert::set_rng_seed(args.seed);
    std::mt19937_64 rng(args.seed);

    // --- Build metadata ---
    uint64_t timestamp = get_current_unix_ns();
    std::string compiler_str = std::string(qert::COMPILER_ID) + "-" + qert::COMPILER_VERSION;

    qert::RunMetadata metadata(args.circuit_family, args.num_qubits, args.depth, args.mapping,
                               args.seed, qert::GIT_COMMIT, compiler_str,
                               qert::COMPILER_FLAGS_DEFAULT, detect_cpu_arch(), detect_cpu_model(),
                               timestamp);

    // --- Build circuit ---
    if (args.circuit_family != "brickwall_1d")
    {
        std::fprintf(stderr, "Error: unknown circuit family '%s'\n", args.circuit_family.c_str());
        return 1;
    }

    qert::Circuit circuit = qert::Circuit::brickwall_1d(args.num_qubits, args.depth);
    circuit.apply_qubit_mapping(args.mapping);

    // --- Initialize simulators ---
    qert::Statevector sv(args.num_qubits);
    qert::MpsSimulator mps(args.num_qubits, args.chi_max);

    // --- Open output file ---
    std::string metadata_json = metadata.to_json();
    std::FILE *out = std::fopen(args.output_path.c_str(), "w");
    if (!out)
    {
        std::fprintf(stderr, "Failed to open: %s\n", args.output_path.c_str());
        return 1;
    }

    // Metadata header (same JSON format as statevector).
    std::string compact_json;
    for (char c : metadata_json)
    {
        if (c == '\n')
            compact_json += ' ';
        else if (c != '\r')
            compact_json += c;
    }
    std::fprintf(out, "# %s\n", compact_json.c_str());

    // MPS-specific CSV header.
    std::fprintf(out, "depth,entropy_nats,chi_max,avg_chi\n");

    // --- Execute circuit ---
    qert::EventID event_id = 0;

    for (uint32_t g = 0; g < circuit.num_gates(); ++g)
    {
        const auto &gate = circuit.gate(g);

        auto mat = qert::generate_random_su4(rng);
        qert::apply_two_qubit_unitary(sv.data(), sv.num_qubits(), gate.control, gate.target,
                                      mat.data());
        mps.apply_two_qubit_gate(gate.control, gate.target, mat.data());

        bool is_last = (gate.gate_idx == circuit.layer_size(gate.depth) - 1);
        bool is_even = (gate.depth % 2 == 0);
        uint32_t subsystem_k = args.num_qubits / 2;

        if (is_last && is_even && subsystem_k <= qert::MAX_ENTROPY_SUBSYSTEM)
        {
            double entropy = qert::compute_half_chain_entropy(sv.data(), sv.num_qubits());
            uint32_t chi = mps.max_bond_dimension();
            double avg_chi = mps.avg_bond_dimension();

            std::fprintf(out, "%u,%.12f,%u,%.3f\n", gate.depth, entropy, chi, avg_chi);
        }

        event_id++;
    }

    std::fclose(out);

    double final_norm = sv.norm();
    if (std::abs(final_norm - 1.0) > 1e-8)
    {
        std::fprintf(stderr, "Warning: final state norm %.12f deviates\n", final_norm);
    }

    std::printf("Run complete: %u qubits, %u depth, %u gates, %llu events\n", args.num_qubits,
                args.depth, circuit.num_gates(), static_cast<unsigned long long>(event_id));
    std::printf("Output: %s\n", args.output_path.c_str());

    return 0;
}
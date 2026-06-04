// qert — Quantum Execution Runtime Telemetry
// Single-circuit experiment runner.
//
// Usage:
//   qert --num-qubits 16 --depth 48 --seed 42
//        --mapping lexicographic --output results/run001.csv
//
// All parameters are required. Output is a self-describing CSV file
// with metadata header and per-gate telemetry rows.

#include "qert/circuit.hpp"
#include "qert/common.hpp"
#include "qert/entropy.hpp"
#include "qert/gates.hpp"
#include "qert/hardware.hpp"
#include "qert/seed.hpp"
#include "qert/statevector.hpp"
#include "qert/telemetry.hpp"
#include <build_info.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace
{

// --- CLI Argument Parsing ---

struct CliArgs
{
    uint32_t num_qubits = 0;
    uint32_t depth = 0;
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
        {
            args.num_qubits = static_cast<uint32_t>(std::atoi(argv[++i]));
        }
        else if (arg == "--depth" && i + 1 < argc)
        {
            args.depth = static_cast<uint32_t>(std::atoi(argv[++i]));
        }
        else if (arg == "--seed" && i + 1 < argc)
        {
            args.seed = static_cast<uint64_t>(std::atoll(argv[++i]));
        }
        else if (arg == "--mapping" && i + 1 < argc)
        {
            args.mapping = argv[++i];
        }
        else if (arg == "--output" && i + 1 < argc)
        {
            args.output_path = argv[++i];
        }
        else if (arg == "--circuit-family" && i + 1 < argc)
        {
            args.circuit_family = argv[++i];
        }
        else if (arg == "--help" || arg == "-h")
        {
            std::printf("qert — Quantum Execution Runtime Telemetry\n"
                        "\n"
                        "Usage:\n"
                        "  qert --num-qubits N --depth D --seed S --mapping M --output PATH\n"
                        "\n"
                        "Required arguments:\n"
                        "  --num-qubits N    Number of qubits (1-%u)\n"
                        "  --depth D         Circuit depth (number of layers)\n"
                        "  --seed S          RNG seed (non-zero)\n"
                        "  --mapping M       Qubit mapping: lexicographic, gray, locality_aware\n"
                        "  --output PATH     Output CSV file path\n"
                        "\n"
                        "Optional arguments:\n"
                        "  --circuit-family F  Circuit family (default: brickwall_1d)\n"
                        "  --help, -h          Show this message\n",
                        qert::MAX_QUBITS);
            return args; // valid == false
        }
        else
        {
            std::fprintf(stderr, "Unknown argument: %s\n", arg.c_str());
            return args;
        }
    }

    // Validate required arguments.
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

// --- CPU Detection ---

std::string detect_cpu_model()
{
    // Try to read from /proc/cpuinfo (Linux) or sysctl (macOS).
    // Returns "unknown" if detection fails.

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
                std::string model = colon + 2; // Skip ": "
                // Trim trailing newline.
                while (!model.empty() && (model.back() == '\n' || model.back() == '\r'))
                {
                    model.pop_back();
                }
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
        {
            model.pop_back();
        }
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
#elif defined(__arm__)
    return "arm";
#else
    return "unknown";
#endif
}

uint64_t get_current_unix_ns()
{
    auto now = std::chrono::system_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    return static_cast<uint64_t>(ns);
}

} // anonymous namespace

// ============================================================================
// Main
// ============================================================================

int main(int argc, char **argv)
{
    // --- Parse arguments ---
    CliArgs args = parse_args(argc, argv);
    if (!args.valid)
    {
        return 1;
    }

    // --- Initialize hardware counters ---
    qert::HardwareProfiler profiler;

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

    // --- Initialize statevector ---
    qert::Statevector sv(args.num_qubits);

    // --- Initialize telemetry ---
    std::string metadata_json = metadata.to_json();
    qert::TelemetryRecorder recorder(args.output_path, metadata_json);

    // --- Execute circuit ---
    qert::EventID event_id = 0;

    for (uint32_t i = 0; i < circuit.num_gates(); ++i)
    {
        const auto &gate = circuit.gate(i);

        // --- Pre-gate telemetry ---
        qert::TelemetryEvent ev;
        ev.event_id = event_id++;
        ev.depth = gate.depth;
        ev.gate_idx = gate.gate_idx;

        // Software-defined locality metrics.
        ev.working_set_kb = qert::estimate_working_set_kb(sv.data(), sv.num_qubits());
        ev.stride_entropy =
            qert::compute_stride_entropy(sv.data(), sv.num_qubits(), gate.control, gate.target);

        // Hardware counters.
        auto start_hw = profiler.read();

        // Timing: measure gate application.
        auto t_start = std::chrono::high_resolution_clock::now();

        // --- Apply gate ---
        if (gate.type == qert::GateType::RANDOM_SU4)
        {
            auto mat = qert::generate_random_su4(rng);
            qert::apply_two_qubit_unitary(sv.data(), sv.num_qubits(), gate.control, gate.target,
                                          mat.data());
        }
        // (Other gate types handled here when circuit supports them.)

        auto t_end = std::chrono::high_resolution_clock::now();
        ev.execution_time_ns = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count());

        // Hardware counters: read after gate, compute delta.
        auto end_hw = profiler.read();
        ev.l3_misses_delta = end_hw.l3_misses - start_hw.l3_misses;
        ev.tlb_misses_delta = end_hw.tlb_misses - start_hw.tlb_misses;

        // --- Entropy sampling ---
        // Compute half-chain entropy at even layers, after the last gate
        // in that layer, and only if the subsystem is small enough.
        bool is_last_in_layer = (gate.gate_idx == circuit.layer_size(gate.depth) - 1);
        bool is_even_layer = (gate.depth % 2 == 0);
        uint32_t subsystem_k = args.num_qubits / 2;

        if (is_last_in_layer && is_even_layer && subsystem_k <= qert::MAX_ENTROPY_SUBSYSTEM)
        {
            ev.half_chain_entropy = qert::compute_half_chain_entropy(sv.data(), sv.num_qubits());
        }
        else
        {
            ev.half_chain_entropy = std::nan("");
        }

        // --- Record ---
        recorder.record(ev);
    }

    // --- Finalize ---
    recorder.close();

    // --- Validate final state ---
    double final_norm = sv.norm();
    if (std::abs(final_norm - 1.0) > 1e-8)
    {
        std::fprintf(stderr, "Warning: final state norm %.12f deviates from 1.0\n", final_norm);
    }

    std::printf("Run complete: %u qubits, %u depth, %u gates, %llu events\n", args.num_qubits,
                args.depth, circuit.num_gates(), static_cast<unsigned long long>(event_id));

    std::printf("Output: %s\n", args.output_path.c_str());

    return 0;
}
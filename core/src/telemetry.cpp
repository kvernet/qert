#include "qert/telemetry.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace qert
{

TelemetryRecorder::TelemetryRecorder(const std::string &output_path,
                                     const std::string &metadata_json)
    : output_path_(output_path), file_(nullptr), event_count_(0), closed_(false)
{
    file_ = std::fopen(output_path_.c_str(), "w");
    if (!file_)
    {
        throw std::runtime_error("Failed to open telemetry file: " + output_path_);
    }

    // Write metadata as a JSON comment line.
    // The '#' prefix makes it a comment for CSV tools but preserves the JSON.
    std::string compact_json;
    for (char c : metadata_json)
    {
        if (c == '\n')
        {
            compact_json += ' '; // Replace newline with space.
        }
        else if (c == '\r')
        {
            // Skip carriage returns.
        }
        else
        {
            compact_json += c;
        }
    }

    // Write metadata as a single JSON comment line.
    std::fprintf(file_, "# %s\n", compact_json.c_str());

    // Write CSV header.
    std::fprintf(file_, "event_id,depth,gate_idx,execution_time_ns,"
                        "l3_misses_delta,tlb_misses_delta,"
                        "working_set_kb,stride_entropy,half_chain_entropy\n");
}

TelemetryRecorder::~TelemetryRecorder()
{
    if (!closed_)
    {
        close();
    }
}

void TelemetryRecorder::record(const TelemetryEvent &event)
{
    assert(file_ != nullptr);
    assert(!closed_);

    // Write CSV row.
    // half_chain_entropy: use "nan" for NaN to signal "not sampled".
    // Analysis scripts parse this as a nullable float.
    if (std::isnan(event.half_chain_entropy))
    {
        std::fprintf(file_, "%llu,%u,%u,%llu,%llu,%llu,%llu,%.6f,nan\n",
                     static_cast<unsigned long long>(event.event_id),
                     static_cast<unsigned>(event.depth), static_cast<unsigned>(event.gate_idx),
                     static_cast<unsigned long long>(event.execution_time_ns),
                     static_cast<unsigned long long>(event.l3_misses_delta),
                     static_cast<unsigned long long>(event.tlb_misses_delta),
                     static_cast<unsigned long long>(event.working_set_kb), event.stride_entropy);
    }
    else
    {
        std::fprintf(file_, "%llu,%u,%u,%llu,%llu,%llu,%llu,%.6f,%.12f\n",
                     static_cast<unsigned long long>(event.event_id),
                     static_cast<unsigned>(event.depth), static_cast<unsigned>(event.gate_idx),
                     static_cast<unsigned long long>(event.execution_time_ns),
                     static_cast<unsigned long long>(event.l3_misses_delta),
                     static_cast<unsigned long long>(event.tlb_misses_delta),
                     static_cast<unsigned long long>(event.working_set_kb), event.stride_entropy,
                     event.half_chain_entropy);
    }

    ++event_count_;
}

void TelemetryRecorder::close()
{
    if (file_ && !closed_)
    {
        std::fclose(file_);
        file_ = nullptr;
        closed_ = true;
    }
}

// ============================================================================
// Software-Defined Locality Metrics (Phase 1 stubs)
// ============================================================================

uint64_t estimate_working_set_kb(const Complex * /*sv*/, uint32_t num_qubits)
{
    // Stub: returns total statevector size.
    // Phase 2: track actual 4KB pages touched via mincore or address tracing.
    uint64_t bytes = (1ULL << num_qubits) * sizeof(Complex);
    return bytes / 1024;
}

double compute_stride_entropy(const Complex * /*sv*/, uint32_t /*num_qubits*/, Qubit /*control*/,
                              Qubit /*target*/
)
{
    // Stub: returns zero.
    // Phase 2: compute Shannon entropy of memory stride distribution.
    return 0.0;
}

} // namespace qert
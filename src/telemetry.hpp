#pragma once

#include "common.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace qert
{

    // --- Per-gate telemetry event ---
    // One event recorded for each gate application during circuit execution.
    // This is the raw data that tests the entanglement-execution hypothesis.

    struct TelemetryEvent
    {
        EventID event_id;           // Monotonically increasing event counter
        Depth depth;                // Circuit layer index (0-based)
        GateIndex gate_idx;         // Gate index within this layer
        uint64_t execution_time_ns; // Wall-clock time for this gate application
        uint64_t l3_misses_delta;   // L3 cache misses since previous event
        uint64_t tlb_misses_delta;  // Data TLB misses since previous event
        uint64_t working_set_kb;    // Unique 4KB pages accessed since circuit start
        double stride_entropy;      // Shannon entropy of memory stride distribution
        double half_chain_entropy;  // Von Neumann entropy of half-chain (NaN if not sampled)
    };

    // --- Telemetry recorder ---
    // Collects per-gate events and writes them to a CSV file matching
    // the telemetry schema in telemetry_schema/.
    //
    // Usage:
    //   TelemetryRecorder recorder("output.csv", metadata_json_string);
    //   for each gate:
    //       TelemetryEvent event = { ... };
    //       recorder.record(event);
    //   recorder.close();

    class TelemetryRecorder
    {
    public:
        // Opens the output file and writes the metadata header.
        // metadata_json is the serialized RunMetadata::to_json() string.
        TelemetryRecorder(
            const std::string &output_path,
            const std::string &metadata_json);

        ~TelemetryRecorder();

        // Record a single gate event. Thread-safe for single-threaded use only.
        void record(const TelemetryEvent &event);

        // Flush and close the output file.
        void close();

        // Number of events recorded so far.
        uint64_t event_count() const { return event_count_; }

    private:
        std::string output_path_;
        std::FILE *file_;
        uint64_t event_count_;
        bool closed_;
    };

} // namespace qert
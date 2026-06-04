#pragma once

// ============================================================================
// qert::HardwareCounters — Hardware performance counter values.
// qert::HardwareProfiler — PAPI-based hardware counter measurement.
//
// Two implementations exist, selected at compile time:
//   QERT_HAS_PAPI defined  → Real PAPI counters.
//   QERT_HAS_PAPI undefined → Stub: all counters return zero.
//
// Usage:
//   #include <qert/hardware.hpp>
//
//   qert::HardwareProfiler profiler;
//   auto before = profiler.read();
//   // ... code to measure ...
//   auto after = profiler.read();
//   uint64_t l3_delta = after.l3_misses - before.l3_misses;
//
// Counter Selection (PAPI build):
//   1. Try portable presets: PAPI_L3_TCM, PAPI_TLB_DM.
//   2. Fall back to native events:
//      - perf::LLC-LOAD-MISSES, ix86arch::LLC_MISSES (L3 cache)
//      - perf::DTLB-LOAD-MISSES (data TLB)
//   3. If no counter is available, throws std::runtime_error.
//
// Thread Safety:
//   PAPI_library_init is called exactly once via std::call_once.
//   HardwareProfiler instances are not thread-safe; each thread
//   should create its own instance.
//
// References:
//   PAPI: Performance Application Programming Interface
//   http://icl.utk.edu/papi/
// ============================================================================

#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace qert
{

// --- Counter Values ---

struct HardwareCounters
{
    uint64_t l3_misses = 0;  // Last-level cache load misses
    uint64_t tlb_misses = 0; // Data TLB load misses
};

// --- Profiler ---

class HardwareProfiler
{
  public:
    // Construct and start counting.
    // Throws std::runtime_error if no counters are available.
    HardwareProfiler();

    ~HardwareProfiler();

    // Non-copyable, non-movable.
    HardwareProfiler(const HardwareProfiler &) = delete;
    HardwareProfiler &operator=(const HardwareProfiler &) = delete;

    // Read current counter values.
    // Returns zeros if the profiler failed to start.
    HardwareCounters read() const;

    // Human-readable names of the selected events (for metadata).
    const std::string &l3_event_name() const;
    const std::string &tlb_event_name() const;

  private:
    // One-time PAPI library initialization.
    static void initialize_papi();

    // Try to add a preset event.
    void add_preset(int event_code, int &index, const char *name);

    // Try to add a native event by name.
    void add_native(const char *event_name, int &index, const char *display_name);

#ifdef QERT_HAS_PAPI
    static constexpr int EVENT_SET_NULL = -1;
    int event_set_ = EVENT_SET_NULL; // PAPI event set handle
    bool started_ = false;           // PAPI_start succeeded

    std::vector<int> event_codes_;          // Added event codes
    mutable std::vector<long long> values_; // PAPI_read buffer

    int l3_index_ = -1; // Position in values_
    int tlb_index_ = -1;

    std::string l3_event_name_; // Human-readable event names
    std::string tlb_event_name_;
#endif
};

} // namespace qert
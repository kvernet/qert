#include "qert/hardware.hpp"

#ifdef QERT_HAS_PAPI
#include <papi.h>
#include <stdexcept>
#include <string>

namespace qert
{

// --- One-time PAPI initialization (called once across all instances) ---

void HardwareProfiler::initialize_papi()
{
    static std::once_flag papi_once;

    std::call_once(papi_once,
                   []()
                   {
                       int ret = PAPI_library_init(PAPI_VER_CURRENT);
                       if (ret < 0)
                       {
                           throw std::runtime_error("PAPI_library_init failed: " +
                                                    std::string(PAPI_strerror(ret)));
                       }
                       if (ret != PAPI_VER_CURRENT)
                       {
                           throw std::runtime_error("PAPI version mismatch.");
                       }
                   });
}

// --- Constructor ---

HardwareProfiler::HardwareProfiler()
{
    initialize_papi();

    int ret = PAPI_create_eventset(&event_set_);
    if (ret != PAPI_OK)
    {
        throw std::runtime_error("PAPI_create_eventset failed: " + std::string(PAPI_strerror(ret)));
    }

    // Prefer portable preset events.
    add_preset(PAPI_L3_TCM, l3_index_, "PAPI_L3_TCM");
    add_preset(PAPI_TLB_DM, tlb_index_, "PAPI_TLB_DM");

    // Optional native fallbacks.
    if (l3_index_ < 0)
    {
        add_native("perf::LLC-LOAD-MISSES", l3_index_, "perf::LLC-LOAD-MISSES");
        add_native("ix86arch::LLC_MISSES", l3_index_, "ix86arch::LLC_MISSES");
    }

    if (tlb_index_ < 0)
    {
        add_native("perf::DTLB-LOAD-MISSES", tlb_index_, "perf::DTLB-LOAD-MISSES");
    }

    if (event_codes_.empty())
    {
        PAPI_destroy_eventset(&event_set_);
        throw std::runtime_error("No supported hardware counters found.");
    }

    values_.resize(event_codes_.size());

    ret = PAPI_start(event_set_);
    if (ret != PAPI_OK)
    {
        PAPI_destroy_eventset(&event_set_);
        throw std::runtime_error("PAPI_start failed: " + std::string(PAPI_strerror(ret)));
    }

    started_ = true;
}

// --- Destructor ---

HardwareProfiler::~HardwareProfiler()
{
    if (event_set_ != PAPI_NULL)
    {
        if (started_)
        {
            std::vector<long long> dummy(values_.size());
            PAPI_stop(event_set_, dummy.data());
        }

        PAPI_cleanup_eventset(event_set_);
        PAPI_destroy_eventset(&event_set_);
    }
}

// --- Read counters ---

HardwareCounters HardwareProfiler::read() const
{
    HardwareCounters result;

    if (!started_)
        return result;

    std::vector<long long> values(values_.size());

    int ret = PAPI_read(event_set_, values.data());
    if (ret != PAPI_OK)
        return result;

    if (l3_index_ >= 0)
    {
        result.l3_misses = static_cast<uint64_t>(values[l3_index_]);
    }

    if (tlb_index_ >= 0)
    {
        result.tlb_misses = static_cast<uint64_t>(values[tlb_index_]);
    }

    return result;
}

const std::string &HardwareProfiler::l3_event_name() const
{
    return l3_event_name_;
}
const std::string &HardwareProfiler::tlb_event_name() const
{
    return tlb_event_name_;
}

// --- Private helpers ---

void HardwareProfiler::add_preset(int event_code, int &index, const char *name)
{
    if (index >= 0)
        return;

    if (PAPI_query_event(event_code) != PAPI_OK)
        return;

    int ret = PAPI_add_event(event_set_, event_code);
    if (ret != PAPI_OK)
        return;

    index = static_cast<int>(event_codes_.size());
    event_codes_.push_back(event_code);

    if (event_code == PAPI_L3_TCM)
        l3_event_name_ = name;

    if (event_code == PAPI_TLB_DM)
        tlb_event_name_ = name;
}

void HardwareProfiler::add_native(const char *event_name, int &index, const char *display_name)
{
    if (index >= 0)
        return;

    if (PAPI_query_named_event(event_name) != PAPI_OK)
        return;

    int ret = PAPI_add_named_event(event_set_, event_name);
    if (ret != PAPI_OK)
        return;

    index = static_cast<int>(event_codes_.size());
    event_codes_.push_back(0);

    if (&index == &l3_index_)
        l3_event_name_ = display_name;

    if (&index == &tlb_index_)
        tlb_event_name_ = display_name;
}

} // namespace qert

#else // !QERT_HAS_PAPI — stub implementation

namespace qert
{

HardwareProfiler::HardwareProfiler()
{
    // No-op: PAPI not compiled in.
}

HardwareProfiler::~HardwareProfiler()
{
    // No-op: PAPI not compiled in.
}

HardwareCounters HardwareProfiler::read() const
{
    return {}; // Returns zeros.
}

const std::string &HardwareProfiler::l3_event_name() const
{
    static const std::string empty;
    return empty;
}
const std::string &HardwareProfiler::tlb_event_name() const
{
    static const std::string empty;
    return empty;
}

void HardwareProfiler::initialize_papi() {}
void HardwareProfiler::add_preset(int, int &, const char *) {}
void HardwareProfiler::add_native(const char *, int &, const char *) {}

} // namespace qert

#endif // QERT_HAS_PAPI
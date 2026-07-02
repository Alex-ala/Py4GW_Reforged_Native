#pragma once

#include "base/error_handling.h"

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

#include <windows.h>

namespace PY4GW {

// Per-metric rolling history. Parity port of the legacy MetricData: samples are
// throttled to one averaged entry every 6 frames, kept in a revolving buffer.
struct MetricData {
    static const int MAX_SAMPLES = 600;
    double samples[MAX_SAMPLES] = {0};
    int head = 0;
    bool full = false;

    // Throttle state
    uint64_t last_frame_id = 0;
    double accumulator = 0;
    int frames_in_window = 0;

    void push_frame_throttled(uint64_t current_frame, double ms);
    size_t count() const { return full ? MAX_SAMPLES : head; }
};

// { min, avg, p50, p95, p99, max }
using MetricSummary = std::tuple<double, double, double, double, double, double>;

// Perf-counter store. Standalone: it does not depend on the callback scheduler.
// Callers pass the current frame stamp to End (as the legacy code passed
// frame_id_timestamp), keeping this class free of System/loop dependencies.
//
// Deviation from legacy: an internal mutex guards the metric maps, because
// reforged drives the profiler from both the update thread and the render
// thread; the legacy single-TU version had no lock and raced.
class Profiler {
public:
    static void Start(const std::string& name);
    static void End(uint64_t frame_id, const std::string& name);

    static MetricSummary CalculateReport(const std::string& metric_name);
    static std::vector<std::tuple<std::string, double, double, double, double, double, double>> CalculateReportAll();
    static std::vector<std::string> GetMetricNames();
    static std::vector<double> GetMetricHistory(const std::string& name);
    static void Reset();

private:
    struct StartPoint {
        LARGE_INTEGER start_time;
    };

    static std::mutex& Mutex();
    static std::map<std::string, StartPoint>& ActiveStarts();
    static std::map<std::string, MetricData>& History();

    static inline LARGE_INTEGER frequency_ = {0};
    static inline bool freq_init_ = false;
};

}  // namespace PY4GW

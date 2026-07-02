#include "base/error_handling.h"

#include "profiler/profiler.h"

#include <algorithm>

namespace PY4GW {

void MetricData::push_frame_throttled(uint64_t current_frame, double ms) {
    accumulator += ms;
    frames_in_window++;

    // Trigger every 6th frame
    if (frames_in_window >= 6) {
        const double avg_load = accumulator / frames_in_window;

        samples[head] = avg_load;
        head = (head + 1) % MAX_SAMPLES;
        if (head == 0) {
            full = true;
        }

        accumulator = 0;
        frames_in_window = 0;
        last_frame_id = current_frame;
    }
}

std::mutex& Profiler::Mutex() {
    static std::mutex mutex;
    return mutex;
}

std::map<std::string, Profiler::StartPoint>& Profiler::ActiveStarts() {
    static std::map<std::string, StartPoint> active_starts;
    return active_starts;
}

std::map<std::string, MetricData>& Profiler::History() {
    static std::map<std::string, MetricData> history;
    return history;
}

void Profiler::Start(const std::string& name) {
    std::lock_guard<std::mutex> lock(Mutex());
    if (!freq_init_) {
        QueryPerformanceFrequency(&frequency_);
        freq_init_ = true;
    }
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    ActiveStarts()[name] = {t};
}

void Profiler::End(uint64_t frame_id, const std::string& name) {
    LARGE_INTEGER t_end;
    QueryPerformanceCounter(&t_end);

    std::lock_guard<std::mutex> lock(Mutex());
    auto& active = ActiveStarts();
    auto it = active.find(name);
    if (it != active.end()) {
        const double duration = static_cast<double>(t_end.QuadPart - it->second.start_time.QuadPart) * 1000.0 / frequency_.QuadPart;
        History()[name].push_frame_throttled(frame_id, duration);
        active.erase(it);
    }
}

MetricSummary Profiler::CalculateReport(const std::string& metric_name) {
    std::lock_guard<std::mutex> lock(Mutex());
    auto& history = History();
    auto it = history.find(metric_name);
    if (it == history.end() || it->second.count() == 0) {
        return {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    }

    auto& data = it->second;
    const size_t n = data.count();

    double sum = 0;
    double min_val = data.samples[0];
    for (size_t i = 0; i < n; ++i) {
        sum += data.samples[i];
        if (data.samples[i] < min_val) {
            min_val = data.samples[i];
        }
    }
    const double avg = sum / n;

    std::vector<double> sorted(data.samples, data.samples + n);
    std::sort(sorted.begin(), sorted.end());

    const double p50 = sorted[static_cast<size_t>(n * 0.50)];
    const double p95 = sorted[static_cast<size_t>(n * 0.95)];
    const double p99 = sorted[static_cast<size_t>(n * 0.99)];
    const double max_val = sorted.back();

    return {min_val, avg, p50, p95, p99, max_val};
}

std::vector<std::tuple<std::string, double, double, double, double, double, double>> Profiler::CalculateReportAll() {
    std::vector<std::string> names = GetMetricNames();

    std::vector<std::tuple<std::string, double, double, double, double, double, double>> reports;
    reports.reserve(names.size());
    for (const auto& name : names) {
        const MetricSummary stats = CalculateReport(name);
        reports.emplace_back(
            name,
            std::get<0>(stats),
            std::get<1>(stats),
            std::get<2>(stats),
            std::get<3>(stats),
            std::get<4>(stats),
            std::get<5>(stats));
    }
    return reports;
}

std::vector<std::string> Profiler::GetMetricNames() {
    std::lock_guard<std::mutex> lock(Mutex());
    std::vector<std::string> names;
    for (const auto& pair : History()) {
        names.push_back(pair.first);
    }
    return names;
}

std::vector<double> Profiler::GetMetricHistory(const std::string& name) {
    std::lock_guard<std::mutex> lock(Mutex());
    auto& history = History();
    auto it = history.find(name);
    if (it == history.end()) {
        return {};
    }

    const auto& data = it->second;
    const size_t n = data.count();
    std::vector<double> out;
    out.reserve(n);

    if (!data.full) {
        for (int i = 0; i < data.head; ++i) {
            out.push_back(data.samples[i]);
        }
    } else {
        for (int i = data.head; i < data.MAX_SAMPLES; ++i) {
            out.push_back(data.samples[i]);
        }
        for (int i = 0; i < data.head; ++i) {
            out.push_back(data.samples[i]);
        }
    }
    return out;
}

void Profiler::Reset() {
    std::lock_guard<std::mutex> lock(Mutex());
    ActiveStarts().clear();
    History().clear();
}

}  // namespace PY4GW

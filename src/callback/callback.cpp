#include "base/error_handling.h"

#include "callback/callback.h"

#include "profiler/profiler.h"
#include "system/system.h"

#include <algorithm>

namespace py = pybind11;

namespace PY4GW {

std::mutex& PyCallback::Mutex() {
    static std::mutex mutex;
    return mutex;
}

std::vector<PyCallback::Task>& PyCallback::Tasks() {
    static std::vector<Task> tasks;
    return tasks;
}

CallbackId PyCallback::Register(
    const std::string& name,
    Phase phase,
    py::function fn,
    int priority,
    Context context) {
    std::lock_guard<std::mutex> lock(Mutex());
    auto& tasks = Tasks();

    // Replace by (name, phase, context), keeping id + order.
    for (auto& t : tasks) {
        if (t.name == name && t.phase == phase && t.context == context) {
            t.fn = std::move(fn);
            t.priority = priority;
            return t.id;
        }
    }

    const CallbackId id = next_id_++;
    tasks.push_back(Task{
        id,
        name,
        phase,
        context,
        priority,
        next_order_++,
        std::move(fn)});
    return id;
}

bool PyCallback::RemoveById(CallbackId id) {
    std::lock_guard<std::mutex> lock(Mutex());
    auto& tasks = Tasks();
    auto it = std::remove_if(tasks.begin(), tasks.end(), [&](const Task& t) { return t.id == id; });
    if (it == tasks.end()) {
        return false;
    }
    tasks.erase(it, tasks.end());
    return true;
}

bool PyCallback::RemoveByName(const std::string& name) {
    std::lock_guard<std::mutex> lock(Mutex());
    auto& tasks = Tasks();
    auto it = std::remove_if(tasks.begin(), tasks.end(), [&](const Task& t) { return t.name == name; });
    if (it == tasks.end()) {
        return false;
    }
    tasks.erase(it, tasks.end());
    return true;
}

void PyCallback::RemoveAll() {
    std::lock_guard<std::mutex> lock(Mutex());
    Tasks().clear();
}

bool PyCallback::PauseById(CallbackId id) {
    std::lock_guard<std::mutex> lock(Mutex());
    for (auto& t : Tasks()) {
        if (t.id == id) {
            t.paused = true;
            return true;
        }
    }
    return false;
}

bool PyCallback::ResumeById(CallbackId id) {
    std::lock_guard<std::mutex> lock(Mutex());
    for (auto& t : Tasks()) {
        if (t.id == id) {
            t.paused = false;
            return true;
        }
    }
    return false;
}

bool PyCallback::IsPaused(CallbackId id) {
    std::lock_guard<std::mutex> lock(Mutex());
    for (const auto& t : Tasks()) {
        if (t.id == id) {
            return t.paused;
        }
    }
    return false;
}

bool PyCallback::IsRegistered(CallbackId id) {
    std::lock_guard<std::mutex> lock(Mutex());
    for (const auto& t : Tasks()) {
        if (t.id == id) {
            return true;
        }
    }
    return false;
}

void PyCallback::ExecutePhase(Phase phase, Context context) {
    std::vector<Task*> phase_tasks;
    {
        std::lock_guard<std::mutex> lock(Mutex());
        for (auto& t : Tasks()) {
            if (t.phase == phase && t.context == context && !t.paused) {
                phase_tasks.push_back(&t);
            }
        }
    }

    if (phase_tasks.empty()) {
        return;
    }

    std::sort(phase_tasks.begin(), phase_tasks.end(), [](const Task* a, const Task* b) {
        if (a->priority != b->priority) {
            return a->priority < b->priority;
        }
        return a->order < b->order;
    });

    const uint64_t frame_id = System::GetTickCount64();

    // The runtime keeps the GIL released outside python_runtime calls, so
    // acquire it before invoking any Python callback.
    py::gil_scoped_acquire gil;

    for (Task* t : phase_tasks) {
        const char* ctx_name = (context == Context::Draw) ? "Draw" : "Update";
        ctx_name = (context == Context::Main) ? "Main" : ctx_name;
        const char* phase_suffix;
        switch (phase) {
        case Phase::PreUpdate: phase_suffix = "PreUpdate"; break;
        case Phase::Data: phase_suffix = "Data"; break;
        default: phase_suffix = "Update"; break;
        }
        const std::string full_prof_name = std::string(ctx_name) + ".Callback." + phase_suffix + "." + t->name;

        Profiler::Start(full_prof_name);
        try {
            t->fn();
        } catch (const py::error_already_set&) {
            PyErr_Print();
        }
        Profiler::End(frame_id, full_prof_name);
    }
}

std::vector<std::tuple<uint64_t, std::string, int, int, int, uint64_t, bool>> PyCallback::GetCallbackInfo() {
    std::lock_guard<std::mutex> lock(Mutex());
    std::vector<std::tuple<uint64_t, std::string, int, int, int, uint64_t, bool>> out;
    auto& tasks = Tasks();
    out.reserve(tasks.size());
    for (const auto& t : tasks) {
        out.emplace_back(
            t.id,
            t.name,
            static_cast<int>(t.phase),
            static_cast<int>(t.context),
            t.priority,
            t.order,
            t.paused);
    }
    return out;
}

void PyCallback::Clear() {
    std::lock_guard<std::mutex> lock(Mutex());
    Tasks().clear();
}

}  // namespace PY4GW

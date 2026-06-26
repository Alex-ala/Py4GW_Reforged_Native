#pragma once

#include "base/error_handling.h"

#include <ctime>

namespace py4gw {

class Timer {
public:
    Timer() = default;

    void start() {
        if (!running_) {
            start_time_ = std::clock();
            running_ = true;
            paused_ = false;
            paused_time_ = 0;
        }
    }

    void stop() {
        running_ = false;
        paused_ = false;
    }

    void Pause() {
        if (running_ && !paused_) {
            paused_time_ = std::clock() - start_time_;
            paused_ = true;
        }
    }

    void Resume() {
        if (running_ && paused_) {
            start_time_ = std::clock() - paused_time_;
            paused_ = false;
        }
    }

    bool isStopped() const {
        return !running_;
    }

    bool isRunning() const {
        return running_ && !paused_;
    }

    bool IsPaused() const {
        return paused_;
    }

    bool HasValidData() const {
        return start_time_ > 0;
    }

    void reset() {
        start_time_ = std::clock();
        running_ = true;
        paused_ = false;
        paused_time_ = 0;
    }

    double getElapsedTime() const {
        if (!running_) {
            return 0.0;
        }
        if (paused_) {
            return (paused_time_ / static_cast<double>(CLOCKS_PER_SEC)) * 1000.0;
        }
        return ((std::clock() - start_time_) / static_cast<double>(CLOCKS_PER_SEC)) * 1000.0;
    }

    bool hasElapsed(double milliseconds) const {
        if (!running_ || paused_) {
            return false;
        }
        return getElapsedTime() >= milliseconds;
    }

private:
    std::clock_t start_time_ = 0;
    std::clock_t paused_time_ = 0;
    bool running_ = false;
    bool paused_ = false;
};

}  // namespace py4gw

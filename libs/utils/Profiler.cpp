#include "Profiler.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>

using json = nlohmann::json;

using namespace std::chrono;
using namespace std;

namespace {

const char* defaultWriteoutPath = nullptr;

inline int64_t getTime_us() {
    return duration_cast<duration<int64_t, micro>>(
               high_resolution_clock::now().time_since_epoch())
        .count();
}

inline int getThreadID() {
    return std::hash<std::thread::id>()(std::this_thread::get_id()) % 128;
}

}  // namespace

std::unique_ptr<Profiler> Profiler::globalProfiler;
std::mutex Profiler::instance_mtx_;
Profiler* Profiler::getProfiler() {
    std::unique_lock<std::mutex> lck(instance_mtx_);
    if (globalProfiler == nullptr) {
        globalProfiler = std::make_unique<Profiler>();
    }
    return globalProfiler.get();
}

void Profiler::record(const TracerEvent& evt) {
    if (!enabled) return;
    std::unique_lock<std::mutex> lck(mtx_);
    events_.push_back(evt);
}

void Profiler::writeOut(const char* path) {
    std::unique_lock<std::mutex> lck(mtx_);
    if (!events_.empty()) {
        std::cout << "Write profiler (" << events_.size()
                                << " records) out to: " << path << std::endl;
        std::ofstream o(path);
        o << "[" << std::endl;
        int64_t min_ts = events_[0].start_;
        for (const auto& event : events_) {
            min_ts = std::min(event.start_, min_ts);
        }
        for (int i = 0; i < static_cast<int>(events_.size()); i++) {
            json jj;
            jj["name"] = events_[i].name_;
            jj["tid"] = events_[i].tid_;
            jj["pid"] = 0;
            jj["ts"] = events_[i].start_ - min_ts;
            jj["dur"] = events_[i].end_ - events_[i].start_;
            jj["cat"] = "pro";
            jj["ph"] = "X";
            o << jj;
            if (i < static_cast<int>(events_.size()) - 1) {
                o << "," << endl;
            } else {
                o << endl;
            }
        }
        o << "]" << std::endl;
        events_.clear();
    }
}

void enabledDefaultWriteout(const char* path) { defaultWriteoutPath = path; }

void triggerProfiler(bool enabled) {
    Profiler::getProfiler()->enabled = enabled;
}

void ProfilerWriteOut(const char* path) {
    Profiler::getProfiler()->writeOut(path);
    Profiler::globalProfiler = nullptr;
}

Profiler::~Profiler() {
    if (defaultWriteoutPath != nullptr) writeOut(defaultWriteoutPath);
}
#ifdef NO_TRACING
Tracer::Tracer(const char* name) {}
Tracer::~Tracer() {}
void Tracer::stop() {}
void Tracer::record() {}
#else
Tracer::Tracer(const char* name)
    : name_(name), start_(getTime_us()), tid_(getThreadID()) {
}
Tracer::Tracer(const char* name, int pid)
    : name_(name), start_(getTime_us()), tid_(pid) {}
Tracer::~Tracer() { stop(); }

void Tracer::stop() {
    if (end_ == -1) {
        end_ = getTime_us();
        record();
    }
}

void Tracer::record() {
    Profiler::getProfiler()->record(TracerEvent{name_, start_, end_, tid_});
}
#endif
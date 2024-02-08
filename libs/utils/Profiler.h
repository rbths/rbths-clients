#pragma once
#include <stdint.h>

#include <memory>
#include <mutex>
#include <vector>
class Tracer {
   public:
    Tracer(const char* name);
    Tracer(const char* name, int pid);
    ~Tracer();
    void stop();

   private:
    const char* name_;
    int64_t start_;
    int64_t end_ = -1;
    int tid_;
    void record();
};

class TracerEvent {
   public:
    const char* name_;
    int64_t start_;
    int64_t end_;
    int tid_;
};

void ProfilerWriteOut(const char* path);
void enabledDefaultWriteout(const char* path);
void triggerProfiler(bool enabled);

class Profiler {
   public:
    static Profiler* getProfiler();
    static std::mutex instance_mtx_;

    void record(const TracerEvent& evt);
    void writeOut(const char* path);
    ~Profiler();
    bool enabled = false;

   private:
    std::vector<TracerEvent> events_;
    static std::unique_ptr<Profiler> globalProfiler;
    std::mutex mtx_;
    friend void ProfilerWriteOut(const char* path);
};

#ifndef NO_TRACING
#define TRACE_TAG Tracer _(__FUNCTION__);
#define TRACE_TAG_S(A) Tracer _(A);
#else
#define TRACE_TAG
#define TRACE_TAG_S(A)
#endif
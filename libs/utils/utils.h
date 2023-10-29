#include <chrono>
using namespace std::chrono;

inline int64_t getTime_us() {
    return duration_cast<duration<int64_t, std::micro>>(
               high_resolution_clock::now().time_since_epoch())
        .count();
}
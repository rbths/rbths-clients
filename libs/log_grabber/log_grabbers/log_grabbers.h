#pragma once
#include "log_grabber.h"
#include <memory>
#include <functional>


namespace rbths {
namespace log_grabber {
class IteratorGrabber: public LogGrabberI {
    private:
        LogIteratorGenerator iterator_generator;
    public:
        virtual ~IteratorGrabber() {};
        IteratorGrabber(LogIteratorGenerator ptr): iterator_generator(ptr) {};
        virtual LogRange getAllLogRange() {
            LogSearchInput input;
            return iterator_generator(input)->getAllLogRange();
        }
        virtual void searchLog(const LogSearchInput& input, LogRange& searched_range, LogRange& returned_range, std::vector<LogEntry> &results, std::vector<std::pair<int32_t, int32_t>> &distribution);
        virtual void searchAndGroup(const LogSearchInput& input, LogRange& searched_range, LogRange& returned_range, std::vector<LogEntry> &results, std::unordered_map<std::string, std::unordered_map<std::string, int>> &groups, std::unordered_map<std::string, std::pair<int64_t, int64_t>> &ranges, std::unordered_set<std::string> &keys, std::vector<std::pair<int32_t, int32_t>> &distribution); 
};

}  // namespace log_grabber
}  // namespace rbths   
#include "log_grabber.h"
namespace rbths {
namespace log_grabber {
namespace plugins {
namespace log_iterators {
namespace journald {


struct JournalDIterator: public LogIteratorI {
    void *journal;
    LogRange searched_range{0, 0};
    int search_limit;
    const LogSearchInput& input;
    uint64_t t0;
    JournalDIterator(const LogSearchInput& input);
    
    LogRange getSearchedRange() {
        return searched_range;
    }
    
    ~JournalDIterator();

    LogRange getAllLogRange();
    virtual std::optional<LogEntry> next();
    static std::unique_ptr<LogIteratorI> create(const LogSearchInput& input);
    static const char* getName() {
        return "journald";
    }
};


}  // namespace journald
}  // namespace log_iterators
}  // namespace plugins
}  // namespace log_grabber
}  // namespace rbths
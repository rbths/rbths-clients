#pragma once
#include <stdint.h>
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#define RBTHS_PLUGIN_LOG_ITERATOR(A) BOOST_DLL_ALIAS(A::create, create_logiterator) BOOST_DLL_ALIAS(A::getName, name_logiterator)

namespace rbths {
namespace log_grabber {
    
struct LogRange {
    uint64_t start;
    uint64_t end;
};

struct LogEntryField {
    std::string key;
    std::string value;
};
struct LogEntry {
    uint64_t timestamp;
    std::string msg;
    std::string service;
    std::string hostname;
    std::string unit;
    std::vector<LogEntryField> additional_fields;
};
enum SubOps {
    AND,
    OR,
    NOT
};
struct FieldStatement {
    std::string key;
    std::optional<std::vector<std::string>> values_in;
    std::optional<std::string> values_regex;
    bool is_null, is_not_null;
    std::optional<int64_t> greater;
};
struct ConditionStatement;
struct SubStatement {
    std::vector<ConditionStatement> sub_conditions;
    SubOps sub_op;
};

struct ConditionStatement {
    std::optional<SubStatement> sub_condition;
    std::optional<FieldStatement> field_statement;
};
struct LogSearchInput{
    LogRange range;
    uint64_t search_limit;
    uint64_t return_limit;
    std::optional<ConditionStatement> conditions;
    int32_t timeout_ms;
};

#define LOG_GRABBER_TOTAL_COUNT_KEY "_RBT_TOTAL_"
struct LogGrabberI {
    virtual ~LogGrabberI() = default;  ///< As polymorphic base class it has to
                                       ///< have virtual destructor.
    virtual LogRange getAllLogRange() = 0;///< Get the range of the log.
    virtual void searchLog(const LogSearchInput& input, LogRange& searched_range, LogRange& returned_range, std::vector<LogEntry> &results) = 0;
    virtual void searchAndGroup(const LogSearchInput& input, LogRange& searched_range, LogRange& returned_range, std::vector<LogEntry> &results, std::unordered_map<std::string, std::unordered_map<std::string, int>> &groups, std::unordered_map<std::string, std::pair<int64_t, int64_t>> &ranges, std::unordered_set<std::string> &keys) = 0;
};

class LogIteratorI {
  public:
    virtual std::optional<LogEntry> next() = 0;
    virtual LogRange getAllLogRange() = 0;
    virtual LogRange getSearchedRange() = 0;
    LogIteratorI(const LogIteratorI&) = delete;
    LogIteratorI& operator=(const LogIteratorI&) = delete;
    LogIteratorI(LogIteratorI&&) = delete;
    LogIteratorI& operator=(LogIteratorI&&) = delete;
    virtual ~LogIteratorI() = default;
    LogIteratorI() = default;

};

// a function returns an iterator that can be used to iterate over the logs
using LogIteratorGenerator = std::function<std::unique_ptr<LogIteratorI>(const LogSearchInput& input)>;
typedef std::unique_ptr<LogIteratorI> (LogIteratorGeneratorPtr)(const LogSearchInput& input);


}  // namespace log_grabber
}  // namespace rbths   
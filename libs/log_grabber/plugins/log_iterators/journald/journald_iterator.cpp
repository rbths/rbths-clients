#include "journald_iterator.h"
#include <regex>
#include <iostream>
#include <systemd/sd-journal.h>
#include "utils.h"
#include <boost/dll/alias.hpp> // for BOOST_DLL_ALIAS   
using namespace rbths::log_grabber;
using namespace rbths::log_grabber::plugins::log_iterators::journald;

LogEntryField parseDataField(const char* data, const size_t len) {
    for(int i = 0; i < len; i++) {
        if (data[i] == '=') {
            return {std::string(data, i), std::string(data + i + 1, len - i - 1)};
        }
    }
    return {};
}

LogEntryField sd_journal_get_data_parsed(sd_journal *j, const char *field) {
    const void* data;
    size_t size;
    int ret = sd_journal_get_data(j, field, &data, &size);
    if (ret != 0) {
        std::cout << "sd_journal_get_data error " << ret << std::endl;
        return {};
    }
    return parseDataField((const char*)data, size);
}

bool is_regex(const std::string& str) {
    return str.find_first_of(".*+?^$()[]{}|\\") != std::string::npos;
}
const std::map<std::string, std::string> FIELD_NAME_MAP = {
    {"msg", "MESSAGE"},
    {"service", "SYSLOG_IDENTIFIER"},
    {"hostname", "_HOSTNAME"},
    {"unit", "_SYSTEMD_UNIT"}
};
void set_indexed_conditions(const rbths::log_grabber::ConditionStatement& conditions, sd_journal* journal) {
    if (conditions.sub_condition.has_value() && conditions.sub_condition.value().sub_op == SubOps::AND) {
        const auto& sub_condition = conditions.sub_condition.value();
        for (auto& sub_condition : conditions.sub_condition.value().sub_conditions) {
            set_indexed_conditions(sub_condition, journal);
        }
    }
    if (conditions.field_statement.has_value() && conditions.field_statement.value().values_in.has_value()) {
        auto& field_statement = conditions.field_statement.value();
        std::string field_name = FIELD_NAME_MAP.find(field_statement.key) != FIELD_NAME_MAP.end() ? FIELD_NAME_MAP.at(field_statement.key) : field_statement.key;
        for (auto& field : field_statement.values_in.value()) {
            sd_journal_add_match((sd_journal*)journal, (field_name + "=" + field).c_str(), 0);
            printf("setting indexed condition %s\n", (field_name + "=" + field).c_str());
        }
    }
}

JournalDIterator::JournalDIterator(const LogSearchInput& input): input(input) {
    t0 = getTime_us();
    sd_journal_open((sd_journal**)&journal, SD_JOURNAL_LOCAL_ONLY);
    sd_journal_flush_matches((sd_journal*)journal);
    sd_journal_seek_head((sd_journal*)journal);
    
    sd_journal_seek_realtime_usec((sd_journal*)journal, input.range.start);
    searched_range.start = input.range.start;

    if (input.conditions.has_value()) {
        set_indexed_conditions(input.conditions.value(), (sd_journal*)journal);
    }
    sd_journal_restart_data((sd_journal*)journal);
    search_limit = input.search_limit;
}
    
JournalDIterator::~JournalDIterator() {
    sd_journal_close((sd_journal*)journal);
}

LogRange JournalDIterator::getAllLogRange() {
    LogRange range;
    sd_journal *journal;
    sd_journal_open((sd_journal**)&journal, SD_JOURNAL_LOCAL_ONLY);
    sd_journal_flush_matches((sd_journal*)journal);
    sd_journal_seek_head((sd_journal*)journal);
    sd_journal_next((sd_journal*)journal);
    sd_journal_get_realtime_usec((sd_journal*)journal, &range.start);
    sd_journal_seek_tail((sd_journal*)journal);
    sd_journal_next((sd_journal*)journal);
    sd_journal_get_realtime_usec((sd_journal*)journal, &range.end);
    sd_journal_close((sd_journal*)journal);
    return range;
}

std::optional<LogEntry> JournalDIterator::next() {
    if ((getTime_us() - t0) > input.timeout_ms * 1000) {
        return {};
    }
    if (search_limit > 0 && sd_journal_next((sd_journal*)journal) > 0) {
        search_limit --;
        LogEntry entry;
        sd_journal_get_realtime_usec((sd_journal*)journal, &entry.timestamp);
        if (input.range.end && entry.timestamp > input.range.end) {
            return {};
        }
        searched_range.end = entry.timestamp;
        
        entry.msg = sd_journal_get_data_parsed((sd_journal*)journal, "MESSAGE").value;

        entry.service = sd_journal_get_data_parsed((sd_journal*)journal, "SYSLOG_IDENTIFIER").value;

        entry.hostname = sd_journal_get_data_parsed((sd_journal*)journal, "_HOSTNAME").value;

        entry.unit = sd_journal_get_data_parsed((sd_journal*)journal, "_SYSTEMD_UNIT").value;

        const void* data;
        size_t size;
        while(sd_journal_enumerate_data((sd_journal*)journal, (const void**)&data, &size) > 0) {
            LogEntryField field = parseDataField((const char*)data, size);

            if (field.key == "" || field.key == "MESSAGE" || field.key == "SYSLOG_IDENTIFIER" || field.key == "_HOSTNAME" || field.key == "_SYSTEMD_UNIT") {
                continue;
            }
            entry.additional_fields.push_back(field);
        }
        return entry;
        
    }
    return {};
}

std::unique_ptr<LogIteratorI> JournalDIterator::create(const LogSearchInput& input) {
    return std::make_unique<JournalDIterator>(input);
}
RBTHS_PLUGIN_LOG_ITERATOR(JournalDIterator)
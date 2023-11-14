#include "log_grabbers.h"
#include <iostream>
#include <regex>
#include "utils.h"

namespace rbths {
namespace log_grabber {

bool check_condition(const ConditionStatement& condition,
                     const LogEntry& entry) {
  if (condition.sub_condition.has_value()) {
    const auto& sub_condition = condition.sub_condition.value();
    switch (sub_condition.sub_op) {
      case SubOps::AND:
        for (auto& sub_condition :
             condition.sub_condition.value().sub_conditions) {
          if (!check_condition(sub_condition, entry)) {
            return false;
          }
        }
        return true;
      case SubOps::OR:
        for (auto& sub_condition :
             condition.sub_condition.value().sub_conditions) {
          if (check_condition(sub_condition, entry)) {
            return true;
          }
        }
        return false;
      case SubOps::NOT:
        for (auto& sub_condition :
             condition.sub_condition.value().sub_conditions) {
          return !check_condition(sub_condition, entry);
        }
    }
  }
  if (condition.field_statement.has_value()) {
    auto& field_statement = condition.field_statement.value();
    if (field_statement.greater.has_value()) {
      if (field_statement.key == "timestamp") {
        return entry.timestamp > field_statement.greater.value();
      }
      if (field_statement.key == "msg_len") {
        return entry.msg.length() > field_statement.greater.value();
      }
      for (auto& field : entry.additional_fields) {
        if (field.key == field_statement.key) {
          if (field.key.find("TIMESTAMP") != std::string::npos) {
            return int64_t(std::stod(field.value) * 1000000) >
                   field_statement.greater.value();
          }
        }
      }
      return false;
    }
    if (field_statement.values_in.has_value()) {
      auto& values = field_statement.values_in.value();
      if (values.size() == 0) {
        return false;
      }

      if (field_statement.key == "msg") {
        return std::find(values.begin(), values.end(), entry.msg) !=
               values.end();
      }
      if (field_statement.key == "service") {
        return std::find(values.begin(), values.end(), entry.service) !=
               values.end();
      }
      if (field_statement.key == "hostname") {
        return std::find(values.begin(), values.end(), entry.hostname) !=
               values.end();
      }
      if (field_statement.key == "unit") {
        return std::find(values.begin(), values.end(), entry.unit) !=
               values.end();
      }
      for (auto& field : entry.additional_fields) {
        if (field.key == field_statement.key) {
          return std::find(values.begin(), values.end(), field.value) !=
                 values.end();
        }
      }
      return false;
    }
    if (field_statement.values_regex.has_value()) {
      auto& regex = field_statement.values_regex.value();
      if (regex == "") {
        return false;
      }
      if (field_statement.key == "msg") {
        std::match_results<std::string::const_iterator> match;
        std::regex regex(field_statement.values_regex.value());
        std::regex_search(entry.msg, match, regex);
        return !match.empty();
      }
      if (field_statement.key == "service") {
        std::match_results<std::string::const_iterator> match;
        std::regex regex(field_statement.values_regex.value());
        std::regex_search(entry.service, match, regex);
        return !match.empty();
      }
      if (field_statement.key == "hostname") {
        std::match_results<std::string::const_iterator> match;
        std::regex regex(field_statement.values_regex.value());
        std::regex_search(entry.hostname, match, regex);
        return !match.empty();
      }
      if (field_statement.key == "unit") {
        std::match_results<std::string::const_iterator> match;
        std::regex regex(field_statement.values_regex.value());
        std::regex_search(entry.unit, match, regex);
        return !match.empty();
      }
      for (auto& field : entry.additional_fields) {
        if (field.key == field_statement.key) {
          std::match_results<std::string::const_iterator> match;
          std::regex regex(field_statement.values_regex.value());
          std::regex_search(field.value, match, regex);
          return !match.empty();
        }
      }
      return false;
    }
    if (field_statement.is_null) {
      if (field_statement.key == "service") {
        return entry.service == "";
      }
      if (field_statement.key == "hostname") {
        return entry.hostname == "";
      }
      if (field_statement.key == "unit") {
        return entry.unit == "";
      }
      for (auto& field : entry.additional_fields) {
        if (field.key == field_statement.key) {
          return field.value == "";
        }
      }
      return true;
    }
    if (field_statement.is_not_null) {
      if (field_statement.key == "service") {
        return entry.service != "";
      }
      if (field_statement.key == "hostname") {
        return entry.hostname != "";
      }
      if (field_statement.key == "unit") {
        return entry.unit != "";
      }
      for (auto& field : entry.additional_fields) {
        if (field.key == field_statement.key) {
          return field.value != "";
        }
      }
      return false;
    }
  }
  return false;
}

void IteratorGrabber::searchLog(
    const LogSearchInput& input,
    LogRange& searched_range,
    LogRange& returned_range,
    std::vector<LogEntry>& results,
    std::vector<std::pair<int32_t, int32_t>>& distribution) {
  uint64_t t0 = getTime_us();
  int64_t return_limit = input.return_limit;
  int64_t offset = input.offset;
  auto iterator = iterator_generator(input);
  while (auto entry = iterator->next()) {
    bool no_more = (getTime_us() - t0) > input.timeout_ms * 1000;
    if (input.conditions.has_value() &&
        !check_condition(input.conditions.value(), entry.value())) {
      if (no_more) {
        break;
      }
      continue;
    }
    if (return_limit > 0 && offset == 0) {
      results.push_back(std::move(entry.value()));
      return_limit--;
    }
    if (offset > 0) {
      offset--;
    }
    if (return_limit <= 0 || no_more) {
      break;
    }
  }
  uint64_t t1 = getTime_us();
  searched_range = iterator->getSearchedRange();
  if (!results.empty()) {
    returned_range = {results.front().timestamp, results.back().timestamp};
  }
  std::cout << "Search took " << (t1 - t0) << " us" << std::endl;
}
inline void update_key_value_range_int(
    const std::string& key,
    const int64_t value,
    std::unordered_map<std::string, std::unordered_map<std::string, int>>&
        groups,
    std::unordered_map<std::string, std::pair<int64_t, int64_t>>& ranges,
    const int limit = 30) {
  if (ranges.find(key) != ranges.end()) {
    ranges[key].first = std::min(ranges[key].first, value);
    ranges[key].second = std::max(ranges[key].second, value);
    if (groups.find(key) == groups.end()) {
      groups[key] = {};
    }
    groups[key][LOG_GRABBER_TOTAL_COUNT_KEY] += 1;
    return;
  }

  if (groups.find(key) == groups.end()) {
    groups[key] = {};
  }
  groups[key][std::to_string(value)] += 1;

  if (groups[key].size() > limit) {
    int total = 0;
    int64_t min = INT64_MAX;
    int64_t max = INT64_MIN;
    for (auto& [k, v] : groups[key]) {
      total += v;
      int64_t val = std::stoll(k);
      min = std::min(min, val);
      max = std::max(max, val);
    }
    groups[key] = {};
    groups[key][LOG_GRABBER_TOTAL_COUNT_KEY] = total;
    ranges[key] = {min, max};
    return;
  }
}

inline void update_key_value_range_str(
    const std::string& key,
    const std::string& value,
    std::unordered_map<std::string, std::unordered_map<std::string, int>>&
        groups,
    std::unordered_set<std::string>& keys,
    const int limit = 100) {
  if (keys.find(key) != keys.end()) {
    return;
  }
  if (groups.find(key) == groups.end()) {
    groups[key] = {};
  }
  groups[key][value] += 1;

  if (groups[key].size() > limit) {
    int total = 0;
    for (auto& [k, v] : groups[key]) {
      total += v;
    }
    groups[key] = {};
    groups[key][LOG_GRABBER_TOTAL_COUNT_KEY] = total;
    keys.insert(key);
    return;
  }
}

void IteratorGrabber::searchAndGroup(
    const LogSearchInput& input,
    LogRange& searched_range,
    LogRange& returned_range,
    std::vector<LogEntry>& results,
    std::unordered_map<std::string, std::unordered_map<std::string, int>>&
        groups,
    std::unordered_map<std::string, std::pair<int64_t, int64_t>>& ranges,
    std::unordered_set<std::string>& keys,
    std::vector<std::pair<int32_t, int32_t>>& distribution) {
  uint64_t t0 = getTime_us();
  uint64_t return_limit = input.return_limit;
  uint64_t offset = input.offset;
  auto iterator = iterator_generator(input);
  std::map<int32_t, int32_t> dist;
  uint64_t id = 0;
  while (auto entry_p = iterator->next()) {
    bool no_more = (getTime_us() - t0) > input.timeout_ms * 1000;
    if (no_more) {
      std::cout << "Timing out" << std::endl;
    }
    if (input.conditions.has_value() &&
        !check_condition(input.conditions.value(), entry_p.value())) {
      if (no_more) {
        break;
      }
      continue;
    }
    auto& entry = entry_p.value();
    update_key_value_range_str("service", entry.service, groups, keys, 10000);
    update_key_value_range_str("hostname", entry.hostname, groups, keys);
    update_key_value_range_str("unit", entry.unit, groups, keys, 10000);
    update_key_value_range_int("timestamp", entry.timestamp, groups, ranges);
    update_key_value_range_int("msg_len", entry.msg.length(), groups, ranges);
    entry.id = id;
    id++;
    for (auto& field : entry.additional_fields) {
      update_key_value_range_str(field.key, field.value, groups, keys);
    }
    dist[entry.timestamp / 1000000] += 1;

    if (return_limit > 0 && offset == 0) {
      results.push_back(std::move(entry));
      return_limit--;
    }
    if (offset > 0) {
      offset--;
    }
    if (no_more) {
      break;
    }
  }
  if (!results.empty()) {
    returned_range = {results.front().timestamp, results.back().timestamp};
  }
  searched_range = iterator->getSearchedRange();
  distribution = std::vector<std::pair<int32_t, int32_t>>(dist.begin(),
                                                          dist.end());
  uint64_t t1 = getTime_us();
  std::cout << "Search took " << (t1 - t0) << " us" << std::endl;
}

}  // namespace log_grabber
}  // namespace rbths
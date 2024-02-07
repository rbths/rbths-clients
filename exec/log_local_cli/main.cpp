#include <iostream>
#include <memory>
#include <string>
#include "log_grabbers.h"
#include "plugin_loader.h"
using namespace rbths::log_grabber;

int main(int argc, char** argv) {
  const char* ppath = std::getenv("LOG_GRABBER_PLUGIN_PATH");
  if (ppath == nullptr) {
    ppath = "/opt/rbths/plugins/log_iterators/";
  }
  auto creator =
      rbths::log_grabber::plugin_loader::getLogIteratorGenerator("journald", ppath);
  if (creator == nullptr) {
    std::cout << "Error loading journald iterator" << std::endl;
    return 1;
  }
  LogSearchInput input;
  input.range.start = 0;
  input.range.end = 18073387179846037;
  // input.service_regex = "systemd\\-.*";
  input.search_limit = 1000000;
  input.return_limit = 100000;
  input.offset = 0;
  ConditionStatement condition;
  condition.sub_condition.emplace();
  condition.sub_condition->sub_op = SubOps::AND;
  condition.sub_condition->sub_conditions.emplace_back();

  condition.sub_condition->sub_conditions.back().field_statement.emplace();
  condition.sub_condition->sub_conditions.back().field_statement->key = "unit";
  condition.sub_condition->sub_conditions.back().field_statement->values_in.emplace();
  condition.sub_condition->sub_conditions.back().field_statement->values_in->push_back("sensor-hub.service");

  condition.sub_condition->sub_conditions.emplace_back();
  
  condition.sub_condition->sub_conditions.back().field_statement.emplace();
  condition.sub_condition->sub_conditions.back().field_statement->key = "msg";
  condition.sub_condition->sub_conditions.back().field_statement->values_regex.emplace();
  condition.sub_condition->sub_conditions.back().field_statement->values_regex = ".*Resu.*";


  input.conditions.emplace(condition);

  // input.msg_regex = "\\d+\\.\\d+\\.\\d+\\.\\d+";
  LogRange searched_range;
  LogRange returned_range;
  std::vector<LogEntry> results;
  rbths::log_grabber::IteratorGrabber grabber(creator);
  std::vector<std::pair<int32_t, int32_t>> distribution;
  grabber.searchLog(input, searched_range, returned_range, results,
                    distribution);
  for (auto& entry : results) {
    std::cout << entry.timestamp << " :: " << entry.service
              << " :: " << entry.unit << " :: " << entry.msg;
    // for(auto& field: entry.additional_fields) {
    //   std::cout << " :: " << field.key << " -> " << field.value;
    // }
    std::cout << std::endl;
  }

  std::cout << "Searched Range:" << searched_range.start << " -> "
            << searched_range.end << " : "
            << (searched_range.end - searched_range.start) << " returned "
            << results.size() << std::endl;

  // std::unordered_map<std::string, std::unordered_map<std::string, int>> groups;
  // std::unordered_map<std::string, std::pair<int64_t, int64_t>> ranges;
  // std::unordered_set<std::string> keys;
  // grabber.searchAndGroup(input, searched_range, returned_range, results, groups,
  //                        ranges, keys, distribution);

  // for (auto& entry : results) {
  //   std::cout << entry.timestamp << " :: " << entry.service
  //             << " :: " << entry.unit << " :: " << entry.msg;
  //   // for(auto& field: entry.additional_fields) {
  //   //   std::cout << " :: " << field.key << " -> " << field.value;
  //   // }
  //   std::cout << std::endl;
  // }

  // for (auto& key : keys) {
  //   std::cout << "String field " << key << " : "
  //             << groups[key][LOG_GRABBER_TOTAL_COUNT_KEY] << std::endl;
  // }

  // for (auto& key : ranges) {
  //   std::cout << "Int field " << key.first << " : [" << key.second.first << ","
  //             << key.second.second
  //             << "]: " << groups[key.first][LOG_GRABBER_TOTAL_COUNT_KEY]
  //             << std::endl;
  // }

  // for (auto& key : groups) {
  //   std::cout << "Distinct " << key.first << " :: ";
  //   for (auto& key2 : key.second) {
  //     std::cout << "[" << key2.first << ":" << key2.second << "] ";
  //   }
  //   std::cout << std::endl;
  // }

  std::cout << "results lens: " << results.size() << std::endl;
  return 0;
}
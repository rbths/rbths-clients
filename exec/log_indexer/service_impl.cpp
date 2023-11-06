#include "service_impl.h"

#include <iostream>
#include <memory>
#include <string>
#include "utils.h"

uint64_t from_Timestamp(const logindexer::Timestamp& ts) {
  return uint64_t(ts.sec()) * 1000000 + ts.us();
}

logindexer::Timestamp to_Timestamp(uint64_t ts) {
  logindexer::Timestamp ret;
  ret.set_sec(ts / 1000000);
  ret.set_us(ts % 1000000);
  return ret;
}
Status LogIndexerServiceImpl::healthCheck(
    ServerContext* context,
    const logindexer::Void* request,
    logindexer::HealthCheckResult* result) {
  const int64_t time = getTime_us();
  result->mutable_localtime()->set_sec(time / 1000000);
  result->mutable_localtime()->set_us(time % 1000000);
  return Status::OK;
}

rbths::log_grabber::ConditionStatement convert_condition(
    const logindexer::ConditionStatement& condition) {
  rbths::log_grabber::ConditionStatement ret;
  if (condition.has_field_statement()) {
    auto& field_statement = ret.field_statement.emplace();
    field_statement.key = condition.field_statement().key();
    if (condition.field_statement().has_greater()) {
      field_statement.greater = condition.field_statement().greater();
    } else if (condition.field_statement().values_in_size() > 0) {
      auto& values_in = field_statement.values_in.emplace();
      values_in.reserve(condition.field_statement().values_in_size());
      for (auto& value : condition.field_statement().values_in()) {
        values_in.push_back(value);
      }
    } else if (condition.field_statement().values_regex() != "") {
      field_statement.values_regex = condition.field_statement().values_regex();
    } else if (condition.field_statement().is_null()) {
      field_statement.is_null = true;
    } else if (condition.field_statement().is_not_null()) {
      field_statement.is_not_null = true;
    } else {
      throw std::runtime_error("Invalid field statement");
    }
  } else if (condition.has_sub_statement()) {
    auto& subs = ret.sub_condition.emplace();
    subs.sub_op =
        (rbths::log_grabber::SubOps)condition.sub_statement().sub_op();
    subs.sub_conditions.reserve(
        condition.sub_statement().sub_statements_size());
    for (auto& sub : condition.sub_statement().sub_statements()) {
      subs.sub_conditions.push_back(convert_condition(sub));
    }
  } else {
    throw std::runtime_error("Invalid condition statement");
  }
  return ret;
}

void populateInput(const SearchQuery* request,
                   rbths::log_grabber::LogSearchInput& input) {
  if (request->has_range()) {
    input.range.start = from_Timestamp(request->range().start());
    input.range.end = from_Timestamp(request->range().end());
  } else {
    input.range.start = 0;
    input.range.end = 0;
  }
  input.search_limit = request->search_limit();
  input.return_limit = request->return_limit();
  input.offset = request->return_offset();

  if (request->has_conditions()) {
    input.conditions = convert_condition(request->conditions());
  }
  input.timeout_ms = request->timeout_ms() == 0 ? 5000 : request->timeout_ms();
}

void populateSearchOutput(
    const rbths::log_grabber::LogRange& searched_range,
    const rbths::log_grabber::LogRange& returned_range,
    const std::vector<rbths::log_grabber::LogEntry>& results,
    const std::vector<std::pair<int32_t, int32_t>>& distribution,
    LogSearchResult* reply) {
  for (auto& entry : results) {
    auto* log_entry = reply->add_log();
    log_entry->mutable_timestamp()->set_sec(entry.timestamp / 1000000);
    log_entry->mutable_timestamp()->set_us(entry.timestamp % 1000000);
    log_entry->set_id(entry.id);
    log_entry->set_msg(entry.msg);
    log_entry->set_service_name(entry.service);
    log_entry->set_hostname(entry.hostname);
    log_entry->set_unit(entry.unit);
    for (auto& field : entry.additional_fields) {
      auto* log_field = log_entry->add_additional_fields();
      log_field->set_key(field.key);
      log_field->set_value(field.value);
    }
  }
  reply->mutable_searched_range()->mutable_start()->CopyFrom(
      to_Timestamp(searched_range.start));
  reply->mutable_searched_range()->mutable_end()->CopyFrom(
      to_Timestamp(searched_range.end));
  reply->mutable_returned_range()->mutable_start()->CopyFrom(
      to_Timestamp(returned_range.start));
  reply->mutable_returned_range()->mutable_end()->CopyFrom(
      to_Timestamp(returned_range.end));
  auto* ts = reply->add_time_series();
  ts->set_name("::all");
  ts->mutable_time_distribution()->Reserve(distribution.size());
  for (auto& dist : distribution) {
    auto* dist_entry = ts->add_time_distribution();
    dist_entry->set_sec(dist.first);
    dist_entry->set_count(dist.second);
  }
}

int populateGroups(
    const std::unordered_map<std::string, std::unordered_map<std::string, int>>&
        groups,
    const std::unordered_map<std::string, std::pair<int64_t, int64_t>>& ranges,
    const std::unordered_set<std::string>& keys,
    logindexer::LogGroupResult* reply) {
  int total_msg = 0;
  for (auto& group : groups) {
    auto* field = reply->add_field_infos();
    field->set_key(group.first);
    if (group.second.find(LOG_GRABBER_TOTAL_COUNT_KEY) != group.second.end()) {
      field->set_total_count(group.second.at(LOG_GRABBER_TOTAL_COUNT_KEY));
      total_msg =
          std::max(total_msg, group.second.at(LOG_GRABBER_TOTAL_COUNT_KEY));

      if (ranges.find(group.first) != ranges.end()) {
        auto* range = field->mutable_ranges();
        range->set_min(ranges.at(group.first).first);
        range->set_max(ranges.at(group.first).second);
      }
    } else {
      int total = 0;
      for (auto& key2 : group.second) {
        auto* value = field->add_values();
        value->set_value(key2.first);
        value->set_count(key2.second);
        total += key2.second;
      }
      field->set_total_count(total);
      total_msg = std::max(total_msg, total);
    }
  }
  return total_msg;
}

Status LogIndexerServiceImpl::searchAndGroup(
    ServerContext* context,
    const SearchQuery* request,
    logindexer::LogGroupResult* reply) {
  std::cout << "Got request for searchAndGroup " << std::endl;
  context->set_compression_level(GRPC_COMPRESS_LEVEL_LOW);
  rbths::log_grabber::LogSearchInput input;
  populateInput(request, input);
  std::cout << "Input timeout " << input.timeout_ms << std::endl;
  rbths::log_grabber::LogRange searched_range;
  rbths::log_grabber::LogRange returned_range;
  std::vector<rbths::log_grabber::LogEntry> results;
  std::unordered_map<std::string, std::unordered_map<std::string, int>> groups;
  std::unordered_map<std::string, std::pair<int64_t, int64_t>> ranges;
  std::unordered_set<std::string> keys;
  std::vector<std::pair<int32_t, int32_t>> distribution;
  grabber.searchAndGroup(input, searched_range, returned_range, results, groups,
                         ranges, keys, distribution);
  populateSearchOutput(searched_range, returned_range, results, distribution,
                       reply->mutable_search_result());
  int total = populateGroups(groups, ranges, keys, reply);
  reply->set_total_count(total);
  return Status::OK;
}

Status LogIndexerServiceImpl::searchLogs(ServerContext* context,
                                         const SearchQuery* request,
                                         LogSearchResult* reply) {
  std::cout << "Got request for search " << std::endl;
  context->set_compression_level(GRPC_COMPRESS_LEVEL_LOW);
  rbths::log_grabber::LogSearchInput input;
  populateInput(request, input);

  rbths::log_grabber::LogRange searched_range;
  rbths::log_grabber::LogRange returned_range;
  std::vector<rbths::log_grabber::LogEntry> results;
  std::vector<std::pair<int32_t, int32_t>> distribution;
  grabber.searchLog(input, searched_range, returned_range, results,
                    distribution);

  populateSearchOutput(searched_range, returned_range, results, distribution,
                       reply);
  return Status::OK;
}
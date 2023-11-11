#include "service_impl.h"

#include <iostream>
#include <memory>
#include <fstream>
#include <filesystem>
#include <thread>
#include <string>
#include "utils.h"
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

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

void populateLogEntry(logindexer::LogEntry* log_entry,
                      const rbths::log_grabber::LogEntry& entry) {
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

void compress_and_save(const std::string& search_id,
                       const std::vector<rbths::log_grabber::LogEntry>& data) {
  std::ofstream out("/tmp/rbths_indexer/searches/" + search_id + ".proto",
                    std::ios_base::out | std::ios_base::binary);
  std::ofstream index_out("/tmp/rbths_indexer/searches/" + search_id + ".idx",
                          std::ios_base::out | std::ios_base::binary);
  uint64_t offset0 = out.tellp();
  for (auto& entry : data) {
    logindexer::LogEntry proto_entry;
    populateLogEntry(&proto_entry, entry);
    uint64_t offset = out.tellp() - offset0;
    index_out.write((const char*)&offset, sizeof(uint64_t));
    proto_entry.SerializeToOstream(&out);
  }
}

void populateSearchOutput(
    const rbths::log_grabber::LogRange& searched_range,
    const rbths::log_grabber::LogRange& returned_range,
    std::vector<rbths::log_grabber::LogEntry>::iterator begin,
    std::vector<rbths::log_grabber::LogEntry>::iterator end,
    const std::vector<std::pair<int32_t, int32_t>>& distribution,
    LogSearchResult* reply) {
  uint64_t t0 = getTime_us();
  for (auto itr = begin; itr != end; itr++) {
    auto* log = reply->add_log();
    populateLogEntry(log, *itr);
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
  std::cout << "Populate output time: " << getTime_us() - t0 << " total" << reply->log_size() <<std::endl;
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
bool LogIndexerServiceImpl::loadSearchResult(
  const SearchQuery* request,
  logindexer::LogSearchResult* reply
) {
  index_index_lock.lock_shared();
  if (log_index_lock.find(request->search_id()) == log_index_lock.end()) {
    index_index_lock.unlock_shared();
    return false;
  }
  std::shared_lock<std::shared_mutex> guard(log_index_lock.find(request->search_id())->second);
  index_index_lock.unlock_shared();
  
  std::cout << "Got request for loadSearchResult with search_id " << request->search_id() << std::endl;
  std::ifstream in(
      "/tmp/rbths_indexer/searches/" + request->search_id() + ".proto",
      std::ios_base::in | std::ios_base::binary);
  std::ifstream index_in(
      "/tmp/rbths_indexer/searches/" + request->search_id() + ".idx",
      std::ios_base::in | std::ios_base::binary);
  if (!in.is_open() || !index_in.is_open()) {
    std::cout << "Error opening file" << std::endl;
    return false;
  }
  int return_offset = request->return_offset();
  int limit = request->return_limit();

  index_in.seekg(return_offset * sizeof(uint64_t));
  std::string buffer;
  buffer.resize(1024 * 1024 * 5);
  uint64_t offset, next_offset = 1;
  index_in.read((char*)&offset, sizeof(uint64_t));
  while (limit > 0) {
    index_in.read((char*)&next_offset, sizeof(uint64_t));
    in.seekg(offset);
    bool index_reach_end = false;
    if (index_in.bad()) {
      index_reach_end = true;
    }
    in.read(buffer.data(),
            index_reach_end ? buffer.size() : next_offset - offset);
    if (in.gcount() == 0 && in.eof()) {
      std::cerr << "Error reading buffer file: " << offset << std::endl;
      return false;
    }
    logindexer::LogEntry entry;
    entry.ParseFromArray(buffer.data(), in.gcount());
    auto* log = reply->add_log();
    log->CopyFrom(entry);
    limit--;
    if (index_reach_end) {
      break;
    }
    offset = next_offset;
  }
  return true;
}
LogIndexerServiceImpl::~LogIndexerServiceImpl() {
  cleanUp(0);
}
void LogIndexerServiceImpl::cleanUp(u_int32_t timeout) {
  uint32_t now = getTime_us() / 1000000;
  for(auto itr = log_index_created_at.begin(); itr != log_index_created_at.end();) {
    if (now - itr->second > timeout || timeout == 0) {
      std::unique_lock<std::shared_mutex> guard(index_index_lock);
      {
        std::unique_lock<std::shared_mutex> guard(log_index_lock[itr->first]);
        std::cout << "Deleting index " << itr->first << std::endl;
        std::remove(("/tmp/rbths_indexer/searches/" + itr->first + ".proto").c_str());
        std::remove(("/tmp/rbths_indexer/searches/" + itr->first + ".idx").c_str());
      }
      log_index_lock.erase(itr->first);
      itr = log_index_created_at.erase(itr);
    } else {
      itr ++;
    }
  }
}

void LogIndexerServiceImpl::schedule_cleanup() {
  std::filesystem::remove_all("/tmp/rbths_indexer/searches");
  std::filesystem::create_directory("/tmp/rbths_indexer/searches");
  std::thread([this]() {
    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(60));
      cleanUp(60 * 60);
    }
  }).detach();
}
Status LogIndexerServiceImpl::searchAndGroup(
    ServerContext* context,
    const SearchQuery* request,
    logindexer::LogGroupResult* reply) {
  std::cout << "Got request for searchAndGroup " << std::endl;
  context->set_compression_level(GRPC_COMPRESS_LEVEL_LOW);
  rbths::log_grabber::LogSearchInput input;
  if (request->search_id() != "") {
    std::cout << "Search Id ignored: " << request->search_id() << std::endl;
  }
  populateInput(request, input);
  std::cout << "Input timeout " << input.timeout_ms << std::endl;
  rbths::log_grabber::LogRange searched_range;
  rbths::log_grabber::LogRange returned_range;
  std::vector<rbths::log_grabber::LogEntry> results;
  std::unordered_map<std::string, std::unordered_map<std::string, int>> groups;
  std::unordered_map<std::string, std::pair<int64_t, int64_t>> ranges;
  std::unordered_set<std::string> keys;
  std::vector<std::pair<int32_t, int32_t>> distribution;
  input.return_limit = 1000000;
  grabber.searchAndGroup(input, searched_range, returned_range, results, groups,
                         ranges, keys, distribution);
  std::cout << "Got results " << results.size() << std::endl;
  auto itr = results.begin() + request->return_offset();
  auto itr_end = itr + request->return_limit();
  populateSearchOutput(searched_range, returned_range, itr, itr_end, distribution,
                       reply->mutable_search_result());
  
  reply->mutable_search_result()->set_search_id(boost::uuids::to_string(boost::uuids::random_generator()()));
  compress_and_save(reply->search_result().search_id(), results);
  {
    std::unique_lock<std::shared_mutex> guard(index_index_lock);
    log_index_created_at[reply->search_result().search_id()] = getTime_us() / 1000000;
    log_index_lock[reply->search_result().search_id()];
  }
  
  int total = populateGroups(groups, ranges, keys, reply);
  reply->set_total_count(total);

  return Status::OK;
}

Status LogIndexerServiceImpl::searchLogs(ServerContext* context,
                                         const SearchQuery* request,
                                         LogSearchResult* reply) {
  context->set_compression_level(GRPC_COMPRESS_LEVEL_LOW);
  if (request->search_id() != "") {
    if (loadSearchResult(request, reply)) {
      return Status::OK;
    }
  }
  std::cout << "Got request for search " << std::endl;
  rbths::log_grabber::LogSearchInput input;
  populateInput(request, input);

  rbths::log_grabber::LogRange searched_range;
  rbths::log_grabber::LogRange returned_range;
  std::vector<rbths::log_grabber::LogEntry> results;
  std::vector<std::pair<int32_t, int32_t>> distribution;
  grabber.searchLog(input, searched_range, returned_range, results,
                    distribution);

  populateSearchOutput(searched_range, returned_range, results.begin(), results.end(), distribution,
                       reply);
  return Status::OK;
}
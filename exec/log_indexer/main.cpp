
#include <iostream>
#include <memory>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/str_format.h"

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include "log_grabbers.h"
#include "logindexer/logindexer.grpc.pb.h"
#include "plugin_loader.h"
#include "utils.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using logindexer::LogIndexer;
using logindexer::LogSearchResult;
using logindexer::SearchQuery;
using rbths::log_grabber::LogIteratorGenerator;

uint64_t from_Timestamp(const logindexer::Timestamp& ts) {
  return uint64_t(ts.sec()) * 1000000 + ts.us();
}

logindexer::Timestamp to_Timestamp(uint64_t ts) {
  logindexer::Timestamp ret;
  ret.set_sec(ts / 1000000);
  ret.set_us(ts % 1000000);
  return ret;
}

ABSL_FLAG(uint16_t, port, 50051, "Server port for the service");
ABSL_FLAG(bool,
          secure,
          true,
          "Run in secure mode, only accept whitelisted client");

// Logic and data behind the server's behavior.
class LogIndexerServiceImpl final : public LogIndexer::Service {
  rbths::log_grabber::IteratorGrabber grabber;

 public:
  LogIndexerServiceImpl(LogIteratorGenerator log_gen) : grabber(log_gen) {}
  Status healthCheck(ServerContext* context,
                     const logindexer::Void* request,
                     logindexer::HealthCheckResult* result) override {
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
        field_statement.values_regex =
            condition.field_statement().values_regex();
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

    if (request->has_conditions()) {
      input.conditions = convert_condition(request->conditions());
    }
    input.timeout_ms =
        request->timeout_ms() == 0 ? 5000 : request->timeout_ms();
  }

  void populateSearchOutput(
      const rbths::log_grabber::LogRange& searched_range,
      const rbths::log_grabber::LogRange& returned_range,
      const std::vector<rbths::log_grabber::LogEntry>& results,
      const std::vector<std::pair<int32_t, int32_t>>& distribution,
      LogSearchResult* reply) {
    int i = 1000;
    for (auto& entry : results) {
      auto* log_entry = reply->add_log();
      log_entry->mutable_timestamp()->set_sec(entry.timestamp / 1000000);
      log_entry->mutable_timestamp()->set_us(entry.timestamp % 1000000);
      log_entry->set_id(i++);
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
      const std::unordered_map<std::string,
                               std::unordered_map<std::string, int>>& groups,
      const std::unordered_map<std::string, std::pair<int64_t, int64_t>>&
          ranges,
      const std::unordered_set<std::string>& keys,
      logindexer::LogGroupResult* reply) {
    int total_msg = 0;
    for (auto& group : groups) {
      auto* field = reply->add_field_infos();
      field->set_key(group.first);
      if (group.second.find(LOG_GRABBER_TOTAL_COUNT_KEY) !=
          group.second.end()) {
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

  Status searchAndGroup(ServerContext* context,
                        const SearchQuery* request,
                        logindexer::LogGroupResult* reply) override {
    std::cout << "Got request for searchAndGroup " << std::endl;
    context->set_compression_level(GRPC_COMPRESS_LEVEL_LOW);
    rbths::log_grabber::LogSearchInput input;
    populateInput(request, input);
    std::cout << "Input timeout " << input.timeout_ms << std::endl;
    rbths::log_grabber::LogRange searched_range;
    rbths::log_grabber::LogRange returned_range;
    std::vector<rbths::log_grabber::LogEntry> results;
    std::unordered_map<std::string, std::unordered_map<std::string, int>>
        groups;
    std::unordered_map<std::string, std::pair<int64_t, int64_t>> ranges;
    std::unordered_set<std::string> keys;
    std::vector<std::pair<int32_t, int32_t>> distribution;
    grabber.searchAndGroup(input, searched_range, returned_range, results,
                           groups, ranges, keys, distribution);
    populateSearchOutput(searched_range, returned_range, results, distribution,
                         reply->mutable_search_result());
    int total = populateGroups(groups, ranges, keys, reply);
    reply->set_total_count(total);
    return Status::OK;
  }

  Status searchLogs(ServerContext* context,
                    const SearchQuery* request,
                    LogSearchResult* reply) override {
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
};
const char* server_key = R"(-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCKHKBnC3eeH4g3
f42RlFkrR5Gpvn79mV5NdRtm+EQOal4//U9A8C86NQejVcyOOt2d1/SxWYr2p3/W
LR3lKHSkwEZUEZwtScDAEmvi5sx8BDGgIIRR9dN6r3toXMgPDEWJhqpJDNaE2EKF
V/Obr+xM8TY1RnejxOdBgXb+Pf9ImoI4tQQsPHFRqE7uoCsl/6KRWG+x38gcS6EO
C2MhPcaRDCte4PN7VOYj4OjqQ/vG8Zqu++vihx0+ZTwbAH/KlMVZQ8fBXLKlbt87
cOerb02P0N0zwozAfnG2tewA7Zsr/wfEB0T76GanoSkS4rkFKfZfrwlsvYpg66/7
89fIfjWpAgMBAAECggEAAJoR1TPJVxIjmub5JAnYXqDT9hWmAi8QKGMVgLwduHix
sEfct8zOzos1x+TYke5t50wY3Un62fV5EDhHnFHyD4K6pmSiQIHvr1aVNkVTsvRQ
uWDS5XQzskyVS4CLVaIeeR7csJn3+FcPPntXjNWLgHqqRg6oeE6kGawPAO9zuepY
JIjaWWFOTI0UndYW13RSGY70Y8gJFrKpBm0IQXDtIfSXEiTV7NO4Vpw/GKUEzwHH
IbeuNjGdSyvyr0rAqFV9IdNZlcvzQ/GUJvnWbMZk30S7FB62pkjZqlPTcKDR074c
tHexfxdwjZyzCaQhJdeE5hqJrED1y7JOmeBDM75hVQKBgQC+YQpMPPzKCP4tr1cW
LAgzubHHP7c33pU9FPzJrXIrTBuD+P7EErBDppj9Kb0QIejp9apn92jTHVvBdfn0
xhGkNmXo1b5VPko7IlhPIfsxqkoUGk62dAmCX4dOfAoKM/XmFYRWAnXlXNNpiEUu
8Los6gLbFdsSOuGYI3CS1md5dQKBgQC5t4ykwGvwnnRYJvj+FDDBMkoAKEWDtMlj
harcQkFE3yWq/3pKAYAbbsmj/MhAQfpMVH26HVLj5+X5uEenGjGWIoJeFEyC/3LX
cXnyAJ1JZKZfKG3+MNFPZWIRkho2c0WckWUCvSVu8a7pcTd2iKvHtMbmcJynhJfo
Uj+FLtJQ5QKBgGtk/pQnS0sFa62F3hCp4i6bsEgAD3E65OFCN5/lciQumM8H5V/i
UwC2uFMH+plhJ6zjYq+nh8CgLSSjUs/b9BO4hCCmguo/a0yUgVCSkMthjsxsUr7m
En8zOHbzzg4XZb3XoYGLfcpXZQOBVdW3Aq9Xiyjf2WcdRCm4xEZxIIoJAoGAElcW
BI8cGb8MLkhq/mxwBCQ7Opi/hKot2S4yGV5Ro8OFrMVeBkDvrcXbE966x928Ih2V
7PNIZElaZLbWHKJxkyoFRdUrWSRw6uXB7SwyXDUcWPldI2UexoB0lteezxHDJhsV
ppeXQsWLSHJtz77fd/FaUxd74O8vMTouG7DsBCECgYEAjPsMe0XUkfagCVnqSszB
LYO73D+8d8g/1/rpDAugKpdawVup/JYh78s4ZuFS85qCOGnE19phOrhSXqFt9dVJ
uC2Ruu8VwlKWtjYSK3cZVq1EwAEYUCKRn7QLc+gfWU5t541JF3vxs/c/FLG9F6zF
q39adTMK7fV1etiCMKRM9HI=
-----END PRIVATE KEY-----)";
const char* server_cert = R"(-----BEGIN CERTIFICATE-----
MIIDszCCApugAwIBAgIUaYSqolfWVMRZEkZj0O51r2reixEwDQYJKoZIhvcNAQEL
BQAwaTELMAkGA1UEBhMCU1MxFTATBgNVBAgMDEVhcnRoIFN5c3RlbTENMAsGA1UE
BwwETW9vbjEPMA0GA1UECgwGUmFiYml0MQ4wDAYDVQQLDAVIb2xlczETMBEGA1UE
AwwKbG9naW5kZXhlcjAeFw0yMzExMDIwNDI2MzNaFw0yNTAzMTYwNDI2MzNaMGkx
CzAJBgNVBAYTAlNTMRUwEwYDVQQIDAxFYXJ0aCBTeXN0ZW0xDTALBgNVBAcMBE1v
b24xDzANBgNVBAoMBlJhYmJpdDEOMAwGA1UECwwFSG9sZXMxEzARBgNVBAMMCmxv
Z2luZGV4ZXIwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCKHKBnC3ee
H4g3f42RlFkrR5Gpvn79mV5NdRtm+EQOal4//U9A8C86NQejVcyOOt2d1/SxWYr2
p3/WLR3lKHSkwEZUEZwtScDAEmvi5sx8BDGgIIRR9dN6r3toXMgPDEWJhqpJDNaE
2EKFV/Obr+xM8TY1RnejxOdBgXb+Pf9ImoI4tQQsPHFRqE7uoCsl/6KRWG+x38gc
S6EOC2MhPcaRDCte4PN7VOYj4OjqQ/vG8Zqu++vihx0+ZTwbAH/KlMVZQ8fBXLKl
bt87cOerb02P0N0zwozAfnG2tewA7Zsr/wfEB0T76GanoSkS4rkFKfZfrwlsvYpg
66/789fIfjWpAgMBAAGjUzBRMB0GA1UdDgQWBBRGQDZeWuG3ICaGDZMdJCUrefvh
kTAfBgNVHSMEGDAWgBRRRWjk1U6zgdcDpg/+B5w2R3k2mTAPBgNVHRMBAf8EBTAD
AQH/MA0GCSqGSIb3DQEBCwUAA4IBAQBK/GGWsDXEHSu7h0+B2EH3sINQ8P+mfZt4
tcxgexHtdvo1bsUDtUYeYj8sCofq7s8VArljNFo0ApkLqOZ6aLFSxQCm37VCrpr9
REAg1ejiIwQxdp2WsXdtE02RAqPVWvnPpLuYXkziADwQKP3iEQWn/1ENSLco5Tri
Rg7IVXdyezynaRx59t2K8I2DrAwFgK7N+UQ+5mF+tdmeelds/DhVR1oZOlfWs9Vt
436l699xi1MXKM61CVfrqGUI/anJIXKlUAMHlf6hVtXtF6sYbwy3aorVyF4KsKy5
/6ElAaTmXPuGKHqwU/KEGveZwiVHmxwyYkaTgoVhlOoDzaVbqqDZ
-----END CERTIFICATE-----)";

const char* root_ca = R"(-----BEGIN CERTIFICATE-----
MIIDrzCCApegAwIBAgIUIeCME+cQJ76u29zNbGYQwGhATvIwDQYJKoZIhvcNAQEL
BQAwZjELMAkGA1UEBhMCU1MxFTATBgNVBAgMDEVhcnRoIFN5c3RlbTENMAsGA1UE
BwwETW9vbjEPMA0GA1UECgwGUmFiYml0MQ4wDAYDVQQLDAVIb2xlczEQMA4GA1UE
AwwHY2VudHJhbDAgFw0yMzExMDIwNTA5NDRaGA8zMDIzMDMwNTA1MDk0NFowZjEL
MAkGA1UEBhMCU1MxFTATBgNVBAgMDEVhcnRoIFN5c3RlbTENMAsGA1UEBwwETW9v
bjEPMA0GA1UECgwGUmFiYml0MQ4wDAYDVQQLDAVIb2xlczEQMA4GA1UEAwwHY2Vu
dHJhbDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKKxkhQiHIXWRdea
BKuaKi4dBIPqOUOrtQBz9NbPQe2/eV+oUMD3NLKeddssbEWhhKDnk5Q9HIzGJ8tI
dmmSV2w1Dnz9C+ZNd+DCN3FQ6lnOGxss2I8os45UV0gA61lBkWXlhsgHTkA14zQs
OOmULwvrGGwwrnlgFgkNUdnY0XGaISWGgnNDVGxiRTEz6bvjfuY3/36Au8mdikD1
26rAi3MLuMb68hOTV9S7J6zXdL0Ek7c+VzbhZmBCWwLHwC8+ux6vn+wz1Wz0UcTB
86EoXJ4H/vzcSQh5SK+r/3CJWHnloET+y93gM6F4gDwv1AKgj5PQkaCvkQ1jyibg
9J9x9I8CAwEAAaNTMFEwHQYDVR0OBBYEFPY3H878FIvJoOIRxVjhkjGB51VfMB8G
A1UdIwQYMBaAFPY3H878FIvJoOIRxVjhkjGB51VfMA8GA1UdEwEB/wQFMAMBAf8w
DQYJKoZIhvcNAQELBQADggEBAJgbmqpr5HBV27J4/DcE7a8uu8PQHuFpnAvy4QtC
Ly3Bq3EbRrwVD1CcLtUsCRshU09vPtE+Ogx7DFQvXZHAJO/zU63w1YTs7IR7fKah
zM4vhROBB1IxTIdX7nsPg1KgMkA2fI65EjlqQSWhFiw0y2VbECAgMc9cpIS9rlpf
fg+KvQEPGTS80Y8f2a79XkbnS5081uEzfCZfDZiHz5mKZE2wK25RAl5rySujhixv
BoMQ6LV6V+gf1XDurjpYhVViD5se/0sLag4xwdkQMrAhK4STHa/YUsPYqqTbOM5+
OOoPoYX588ueO3UqPELpvKx7WaEFwjzmRYCNemQ3XLruHkU=
-----END CERTIFICATE-----)";

void RunServer(uint16_t port) {
  std::string server_address = absl::StrFormat("0.0.0.0:%d", port);
  LogIteratorGenerator log_generator =
      rbths::log_grabber::plugin_loader::getLogIteratorGenerator("journald");
  if (log_generator == nullptr) {
    std::cout << "Error loading journald iterator" << std::endl;
    return;
  }
  LogIndexerServiceImpl service(log_generator);
  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  if (absl::GetFlag(FLAGS_secure)) {
    std::cout << "Running in secure mode" << std::endl;
    grpc::SslServerCredentialsOptions::PemKeyCertPair pkcp = {
        server_key,
        server_cert,
    };
    grpc::SslServerCredentialsOptions ssl_opts;
    ssl_opts.force_client_auth = true;
    ssl_opts.client_certificate_request =
        GRPC_SSL_REQUEST_CLIENT_CERTIFICATE_AND_VERIFY;
    ssl_opts.pem_root_certs = root_ca;
    ssl_opts.pem_key_cert_pairs.push_back(pkcp);

    builder.AddListeningPort(server_address,
                             grpc::SslServerCredentials(ssl_opts));
  } else {
    std::cout << "Running in insecure mode" << std::endl;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  }
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  server->Wait();
}

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  RunServer(absl::GetFlag(FLAGS_port));
  return 0;
}
#include <iostream>
#include <memory>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/str_format.h"

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include "logindexer/logindexer.grpc.pb.h"
#include <grpcpp/completion_queue.h>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::ClientContext;
using logindexer::LogIndexer;
using logindexer::SearchQuery;
using logindexer::LogSearchResult;

ABSL_FLAG(std::string, target, "localhost:50051", "Server address");

SearchQuery query1() {
  SearchQuery query;
  auto& range = *query.mutable_range();
  range.mutable_start()->set_sec(1697922720);
  range.mutable_end()->set_sec(1698009120);
  query.set_search_limit(100000);
  query.set_return_limit(200);
  auto* fs = query.mutable_conditions()->mutable_field_statement();
  fs->set_key("service");
  fs->mutable_values_in()->Add("sshd");
  return query;
}

SearchQuery query2() {
  SearchQuery query;
  
  query.set_search_limit(10000000);
  query.set_return_limit(200);
  auto* fs = query.mutable_conditions()->mutable_sub_statement();//->mutable_field_statement();
  fs->set_sub_op(logindexer::SubOps::AND);
  auto* fs2 = fs->mutable_sub_statements()->Add()->mutable_field_statement();
  fs2->set_key("_EXE");
  fs2->add_values_in("/lib/systemd/systemd-timesyncd");

  auto* fs3 = fs->mutable_sub_statements()->Add()->mutable_sub_statement();
  fs3->set_sub_op(logindexer::SubOps::NOT);
  auto* fs4 = fs3->mutable_sub_statements()->Add()->mutable_field_statement();
  fs4->set_key("msg_len");
  fs4->set_greater(1024);

  auto* fs5 = fs->mutable_sub_statements()->Add()->mutable_field_statement();
  fs5->set_key("msg");
  fs5->set_values_regex(".*\\d+\\.\\d+\\.\\d+\\.\\d+.*");

  return query;
}
int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  std::unique_ptr<LogIndexer::Stub> stub = LogIndexer::NewStub(grpc::CreateChannel(absl::GetFlag(FLAGS_target), grpc::InsecureChannelCredentials()));
  SearchQuery query = query1();

  ClientContext context;
  logindexer::LogGroupResult rst;
  stub->searchAndGroup(&context, query, &rst);
  std::cout << "Log searched: " << rst.total_count() << " [" << rst.search_result().searched_range().start().sec() << "-" << rst.search_result().searched_range().end().sec() << "]" << std::endl;
  std::cout << "Log returned: " << rst.search_result().log_size() << " [" << rst.search_result().returned_range().start().sec() << "-" << rst.search_result().returned_range().end().sec() << "]" << std::endl;
  

  for(int i = 0; i < rst.search_result().log_size(); i++) {
    auto& l = rst.search_result().log(i);
    std::cout << l.timestamp().sec() << ":: ";
    for(auto f: l.additional_fields()) {
      if (f.key() == "stub_source") {
        std::cout << f.value() << " :: ";
      }
    }
    std::cout << l.msg() << std::endl;
  }

  for(auto& field_info: rst.field_infos()) {
    std::cout << "Field " << field_info.key() << " : " << field_info.total_count() << "::";
    if (field_info.values_size() > 0) {
      int i = 0; 
      for(auto& value_info: field_info.values()) {
        std::cout << value_info.value() << " -> " << value_info.count();
        if (i < field_info.values_size() - 1) {
          std::cout << ", ";
        }
        i++;
        if (i > 10) {
          std::cout << "...";
          break;
        }
      }
    } else {
      if (field_info.has_ranges()) {
        std::cout << "[" << field_info.ranges().min() << ", " << field_info.ranges().max() << "]" << std::endl;
      }
    }
    std::cout << std::endl;
  }

  for(auto& dis : rst.search_result().time_distribution()) {
    std::cout << dis.sec() << " -> " << dis.count() << std::endl;
  }
  
  

  return 0;
}



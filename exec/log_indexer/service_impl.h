#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include "log_grabbers.h"
#include "logindexer/logindexer.grpc.pb.h"
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using logindexer::LogIndexer;
using logindexer::LogSearchResult;
using logindexer::SearchQuery;
using rbths::log_grabber::LogIteratorGenerator;

class LogIndexerServiceImpl final : public LogIndexer::Service {
  rbths::log_grabber::IteratorGrabber grabber;

 public:
  LogIndexerServiceImpl(LogIteratorGenerator log_gen) : grabber(log_gen) {}
  Status healthCheck(ServerContext* context,
                     const logindexer::Void* request,
                     logindexer::HealthCheckResult* result) override;

  Status searchAndGroup(ServerContext* context,
                        const SearchQuery* request,
                        logindexer::LogGroupResult* reply) override;

  Status searchLogs(ServerContext* context,
                    const SearchQuery* request,
                    LogSearchResult* reply) override;
};
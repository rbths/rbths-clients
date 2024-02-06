
#include <iostream>
#include <memory>
#include <cstdlib>
#include <fstream>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/str_format.h"

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include "service_impl.h"
#include "plugin_loader.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

const char* getEnv(const char* name, const char* defaultValue) {
  char* value = std::getenv(name);
  if (value == nullptr) {
    return defaultValue;
  }
  return value;
}

int getEnvInt(const char* name, int defaultValue) {
  char* value = std::getenv(name);
  if (value == nullptr) {
    return defaultValue;
  }
  return std::stoi(value);
}

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
  if (getEnvInt("RBTHS_SECURE_MODE", 1)) {
    std::cout << "Running in secure mode" << std::endl;
    std::string root_ca_path = getEnv("RBTHS_ROOT_CA", "");
    std::string server_key_path = getEnv("RBTHS_SERVER_KEY", "");
    std::string server_cert_path = getEnv("RBTHS_SERVER_CERT", "");
    if (root_ca_path.empty() || server_key_path.empty() ||
        server_cert_path.empty()) {
      std::cout << "Missing required environment variables" << std::endl;
      return;
    }

    std::string root_ca;
    std::string server_key;
    std::string server_cert;
    std::ifstream root_ca_file(root_ca_path);
    std::ifstream server_key_file(server_key_path);
    std::ifstream server_cert_file(server_cert_path);
    root_ca.assign((std::istreambuf_iterator<char>(root_ca_file)),
                   (std::istreambuf_iterator<char>()));
    server_key.assign((std::istreambuf_iterator<char>(server_key_file)),
                      (std::istreambuf_iterator<char>()));
    server_cert.assign((std::istreambuf_iterator<char>(server_cert_file)),
                       (std::istreambuf_iterator<char>()));


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
  int port = getEnvInt("RBTHS_SERVER_PORT", 50051);
  RunServer(port);
  return 0;
}
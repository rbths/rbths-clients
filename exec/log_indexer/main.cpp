
#include <iostream>
#include <memory>
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



ABSL_FLAG(uint16_t, port, 45562, "Server port for the service");
ABSL_FLAG(bool,
          secure,
          true,
          "Run in secure mode, only accept whitelisted client");

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
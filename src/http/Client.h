#pragma once

#include "http/http_common.h"
#include <string>
#include <utility>
#include <vector>

namespace http {

struct RequestOptions {
  int connect_timeout_ms = 0; // optional, best-effort
  int read_timeout_ms = 0;    // optional, best-effort
  bool https = false;
  std::vector<std::pair<std::string, std::string>> headers; // extra headers
};

class Client {
public:
  static Response get(const std::string &host, int port, const std::string &path,
                      const RequestOptions &opt = {});

  static Response post(const std::string &host, int port, const std::string &path,
                       const std::string &content_type,
                       const std::string &body,
                       const RequestOptions &opt = {});
};

} // namespace http


#pragma once

#include "http/Client.h"
#include <string>

namespace micro_todo {

struct ServiceConfig {
  std::string host = "localhost";
  int port = 8083; // default auth port
  bool https = false;
};

class AuthClient {
public:
  explicit AuthClient(ServiceConfig cfg) : _cfg(std::move(cfg)) {}

  bool check(const std::string &token, int timeout_ms = 500) const {
    http::RequestOptions opt;
    opt.https = _cfg.https;
    opt.connect_timeout_ms = timeout_ms;
    opt.read_timeout_ms = timeout_ms;
    opt.headers.emplace_back("Authorization", std::string("Bearer ") + token);
    auto resp = http::Client::get(_cfg.host, _cfg.port, "/auth/check", opt);
    return resp.code == 200;
  }

  std::string build_login_url(const std::string &return_url) const {
    // Gateway will redirect client to this URL
    return std::string(_cfg.https ? "https://" : "http://") + _cfg.host + ":" +
           std::to_string(_cfg.port) + "/auth?return=" + http::url_encode(return_url);
  }

private:
  ServiceConfig _cfg;
};

} // namespace micro_todo


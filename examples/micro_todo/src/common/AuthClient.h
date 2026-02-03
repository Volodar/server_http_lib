#pragma once

#include <string>
#include "http/http_common.h"

namespace micro_todo {

struct ServiceConfig {
    std::string host = "localhost";
    std::string port = "8083"; // default auth port
    bool https = false;
};

class AuthClient {
public:
    explicit AuthClient(ServiceConfig cfg) : _cfg(std::move(cfg)) {}
    
    bool check(const std::string &token, int timeout_ms = 500) const {
        auto resp = http::get(_cfg.host.data(), _cfg.port.data(), "/auth/check", "", {
            {"Authorization", "Bearer " + token},
        });
        return resp.code == 200;
    }
    
    std::string build_login_url(const std::string &return_url) const {
        return std::string(_cfg.https ? "https://" : "http://") + _cfg.host + ":" + _cfg.port + "/auth?return=" + http::url_encode(return_url);
    }
    
private:
    ServiceConfig _cfg;
};

} // namespace micro_todo


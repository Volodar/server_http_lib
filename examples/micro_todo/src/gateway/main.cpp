#include "http/ServerApp.h"
#include "http/http.h"
#include "src/common/ports.h"
#include "src/common/AuthClient.h"
#include "jsoncpp/json.h"
#include <fstream>
#include <memory>
#include <string>

using namespace http;

static micro_todo::ServiceConfig load_auth_config() {
    micro_todo::ServiceConfig cfg;
    std::ifstream in("services.json");
    if (in.good()) {
        Json::Value root;
        in >> root;
        if (root.isMember("auth")) {
            const auto &a = root["auth"];
            if (a.isMember("host")) cfg.host = a["host"].asString();
            if (a.isMember("port")) cfg.port = a["port"].asInt();
            if (a.isMember("https")) cfg.https = a["https"].asBool();
        }
    } else {
        cfg.host = "localhost";
        cfg.port = std::to_string(micro_todo::PORT_AUTH);
        cfg.https = false;
    }
    return cfg;
}

class RootHandler : public RequestHandler {
public:
    explicit RootHandler(ServerApp &app, micro_todo::AuthClient auth) : RequestHandler(app), _auth(std::move(auth)) {
        this->set_sequire([&](const Request &req) -> Response {
            std::string token = std::string(req.get_cookie_value("auth_token"));
            if (!token.empty() && _auth.check(token)) {
                return ResponseNone;
            }

            std::string host = std::string(req.get_headers().get("Host"));
            if (host.empty())
                host = "localhost:8080";
            std::string ret = std::string("http://") + host + std::string(req.get_path());
            std::string loc = _auth.build_login_url(ret);
            Response r(302, "");
            r.add_header("Location", loc);
            return r;
        });
    }
    Response handle(const RequestIncoming &) override {
        auto build_service = [](const char *name, int port) {
            Url url{"localhost", port, "/health", false};
            auto resp = request(url, Method::get);
            Json::Value js;
            js["name"] = name;
            js["port"] = port;
            js["status"] = resp.code == 200 ? "up" : "down";
            js["code"] = resp.code;
            return js;
        };
        
        Json::Value js, services;
        js["name"] = "gateway";
        js["services"].append(build_service("users", micro_todo::PORT_USERS));
        js["services"].append(build_service("tasks", micro_todo::PORT_TASKS));
        js["services"].append(build_service("auth", micro_todo::PORT_AUTH));
        
        Response r(200, write_json_to_string(js));
        r.add_header_content_type(http::ContentType::Json);
        return r;
    }
private:
    micro_todo::AuthClient _auth;
};

int main() {
  http::ServerApp app(micro_todo::PORT_GATEWAY);
  auto auth_cfg = load_auth_config();
  micro_todo::AuthClient auth(auth_cfg);
  app.add_endpoint<http::HealthHandler>("/health", http::Method::get);
  app.add_endpoint<RootHandler>("/", http::Method::get, auth);

  app.run(1);
  return 0;
}

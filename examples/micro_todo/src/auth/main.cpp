#include "http/ServerApp.h"
#include "http/http.h"
#include "http/uuid4.h"
#include "src/common/ports.h"
#include <cstdlib>

using namespace http;

namespace {

static const char *kCookieName = "auth_token";
static const char *kAuthTable = "auth_users";

void ensure_tables(http::ServerApp &app) {
  auto mysql = app.get_mysql();
  if (!mysql)
    return;
  const char *schema = std::getenv("MYSQL_DATABASE");
  if (!schema)
    return;
  const std::string create_sql =
      "CREATE TABLE IF NOT EXISTS $0.$1 (\n"
      "  id BIGINT NOT NULL AUTO_INCREMENT PRIMARY KEY,\n"
      "  token VARCHAR(64) NOT NULL UNIQUE,\n"
      "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP\n"
      ")";
  mysql->create_table(schema, kAuthTable, create_sql);
}

class AuthHandler : public RequestHandler {
public:
  explicit AuthHandler(ServerApp &app) : RequestHandler(app) {}
  Response handle(const Request &req) override {
    auto mysql = _app.get_mysql();
    const char *schema = std::getenv("MYSQL_DATABASE");
    if (!mysql || !schema) {
      Response r(500, "{\"error\":\"db-not-configured\"}");
      r.add_header_content_type(ContentType::Json);
      return r;
    }

    std::string token = std::string(req.get_cookie_value(kCookieName));
    if (token.empty()) {
      token = uuid4::generate();
    } else {
      // If cookie exists, verify presence in DB
      auto q = build_query(
          "SELECT id FROM `$0`.`$1` WHERE token='$2' LIMIT 1", schema,
          kAuthTable, token);
      auto res = mysql->query_get(q);
      if (res && res->next()) {
        Response r(200, "{\"status\":\"ok\"}");
        r.add_header("Set-Cookie", std::string(kCookieName) + "=" + token +
                                    "; Path=/; HttpOnly; SameSite=Lax");
        r.add_header_content_type(ContentType::Json);
        return r;
      }
      // else: fallthrough to create
    }

    // Insert new user with token
    auto insert = build_query("INSERT INTO `$0`.`$1` (token) VALUES('$2')",
                              schema, kAuthTable, token);
    mysql->query(insert);

    // Optional redirect back URL
    std::string ret = http::url_decode(std::string(req.get_params().get("return")));
    if (!ret.empty()) {
      Response r(301, "");
      r.add_header("Location", ret);
      r.add_header("Set-Cookie", std::string(kCookieName) + "=" + token +
                                  "; Path=/; HttpOnly; SameSite=Lax");
      return r;
    } else {
      Response r(200, "{\"created\":true}");
      r.add_header("Set-Cookie", std::string(kCookieName) + "=" + token +
                                  "; Path=/; HttpOnly; SameSite=Lax");
      r.add_header_content_type(ContentType::Json);
      return r;
    }
  }
};

class CheckHandler : public RequestHandler {
public:
  explicit CheckHandler(ServerApp &app) : RequestHandler(app) {}
  Response handle(const Request &req) override {
    auto mysql = _app.get_mysql();
    const char *schema = std::getenv("MYSQL_DATABASE");
    if (!mysql || !schema) {
      Response r(500, "{\"error\":\"db-not-configured\"}");
      r.add_header_content_type(ContentType::Json);
      return r;
    }
    // Prefer Authorization: Bearer <token>
    std::string authz = std::string(req.get_headers().get("Authorization"));
    std::string token;
    if (!authz.empty()) {
      const std::string prefix = "Bearer ";
      if (authz.rfind(prefix, 0) == 0) {
        token = authz.substr(prefix.size());
      }
    }
    if (token.empty()) token = std::string(req.get_cookie_value(kCookieName));
    if (token.empty()) token = std::string(req.get_params().get("token"));
    if (token.empty()) {
      Response r(401, "{\"ok\":false}");
      r.add_header_content_type(ContentType::Json);
      return r;
    }
    auto q = build_query(
        "SELECT id FROM `$0`.`$1` WHERE token='$2' LIMIT 1", schema,
        kAuthTable, token);
    auto res = mysql->query_get(q);
    if (res && res->next()) {
      Response r(200, "{\"ok\":true}");
      r.add_header_content_type(ContentType::Json);
      return r;
    }
    Response r(401, "{\"ok\":false}");
    r.add_header_content_type(ContentType::Json);
    return r;
  }
};

} // namespace

int main() {
  http::ServerApp app(micro_todo::PORT_AUTH);
  ensure_tables(app);
  app.add_endpoint<http::HealthHandler>("/health", http::Method::get);
  app.add_endpoint<AuthHandler>("/auth", http::Method::get);
  app.add_endpoint<CheckHandler>("/auth/check", http::Method::get);
  app.run(1);
  return 0;
}

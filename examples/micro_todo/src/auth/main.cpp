#include "http/ServerApp.h"
#include "http/http.h"
#include "http/uuid4.h"
#include "src/common/ports.h"
#include <cstdlib>

using namespace http;


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
    "CREATE TABLE IF NOT EXISTS $0.$1 ("
    "  id BIGINT NOT NULL AUTO_INCREMENT PRIMARY KEY,"
    "  token VARCHAR(64) NOT NULL UNIQUE,"
    "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
    ")";
    mysql->create_table(schema, kAuthTable, create_sql);
}

class AuthHandler : public RequestHandler {
public:
    explicit AuthHandler(ServerApp &app) : RequestHandler(app) {}
    Response handle(const RequestIncoming &req) override {
        auto mysql = _app.get_mysql();
        const char *schema = std::getenv("MYSQL_DATABASE");
        if (!mysql || !schema) {
            Response r(500, R"({"error":"db-not-configured"})");
            r.add_header_content_type(ContentType::Json);
            return r;
        }
        
        std::string token = std::string(req.get_cookie_value(kCookieName));
        if (token.empty()) {
            token = uuid4::generate();
        } else {
            auto q = build_query("SELECT id FROM `$0`.`$1` WHERE token='$2' LIMIT 1", schema, kAuthTable, token);
            auto res = mysql->query_get(q);
            if (res && res->next()) {
                Response r(200, R"({"status":"ok"})");
                r.add_header("Set-Cookie", std::string(kCookieName) + "=" + token + "; Path=/; HttpOnly; SameSite=Lax");
                r.add_header_content_type(ContentType::Json);
                return r;
            }
        }
        
        mysql->query(build_query("INSERT INTO `$0`.`$1` (token) VALUES('$2')", schema, kAuthTable, token));
        std::string ret = http::url_decode(std::string(req.get_params().get("return")));
        if (!ret.empty()) {
            Response r(301, "");
            r.add_header("Location", ret);
            r.add_header("Set-Cookie", std::string(kCookieName) + "=" + token + "; Path=/; HttpOnly; SameSite=Lax");
            return r;
        } else {
            Response r(200, R"({"created":true})");
            r.add_header("Set-Cookie", std::string(kCookieName) + "=" + token + "; Path=/; HttpOnly; SameSite=Lax");
            r.add_header_content_type(ContentType::Json);
            return r;
        }
    }
};

class CheckHandler : public RequestHandler {
public:
    explicit CheckHandler(ServerApp &app) : RequestHandler(app) {}
    Response handle(const RequestIncoming &req) override {
        auto mysql = _app.get_mysql();
        const char *schema = std::getenv("MYSQL_DATABASE");
        if (!mysql || !schema) {
            Response r(500, R"({"error":"db-not-configured"})");
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
        if (token.empty())
            token = std::string(req.get_cookie_value(kCookieName));
        if (token.empty())
            token = std::string(req.get_params().get("token"));
        if (token.empty()) {
            Response r(401, R"({"ok":false})");
            r.add_header_content_type(ContentType::Json);
            return r;
        }
        auto res = mysql->query_get(build_query("SELECT id FROM `$0`.`$1` WHERE token='$2' LIMIT 1", schema, kAuthTable, token));
        if (res && res->next()) {
            Response r(200, R"({"ok":true})");
            r.add_header_content_type(ContentType::Json);
            return r;
        }
        Response r(401, R"({"ok":false})");
        r.add_header_content_type(ContentType::Json);
        return r;
    }
};


int main() {
    http::Log::set_level(http::Log::Level::debug);
    
    http::ServerApp app(micro_todo::PORT_AUTH);
    ensure_tables(app);
    app.add_endpoint<http::HealthHandler>("/health", http::Method::get);
    app.add_endpoint<AuthHandler>("/auth", http::Method::get);
    app.add_endpoint<CheckHandler>("/auth/check", http::Method::get);
    app.run(1);
    return 0;
}

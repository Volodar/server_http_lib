// Пример сайта с эндпоинтами и MySQL
#include "http/http.h"
#if SERVER_WEB_HAVE_MYSQLCONNECTOR == 1
#include "http/mysql_wrapper.h"
#endif


class HttpIndex : public http::RequestHandler
{
public:
    using http::RequestHandler::RequestHandler;
    virtual http::Response handle(const http::RequestIncoming& request) override{
        std::string body = "Hello world\n";
        body += "\nPath: " + std::string(request.get_path()) + "\n";
        body += "\nParams: \n" + request.get_params().to_string() + "\n";
        body += "\nHeaders: \n" + request.get_headers().to_string('\n') + "\n";
        return http::Response(200, body);
    }
};


#if SERVER_WEB_HAVE_MYSQLCONNECTOR == 1
class DbHealth : public http::RequestHandler {
public:
    using http::RequestHandler::RequestHandler;
    virtual http::Response handle(const http::RequestIncoming &request) override {
        auto mysql = _app.get_mysql();
        if (!mysql) {
            return http::Response(503, "mysql not initialized\n");
        }
        try {
            auto res = mysql->query_get("SELECT 1");
            if (res && res->next() && res->getInt(1) == 1) {
                http::Response r(200, "{\"status\":\"ok\"}\n");
                r.add_header_content_type(http::ContentType::Json);
                return r;
            }
            http::Response r(500, "{\"status\":\"fail\"}\n");
            r.add_header_content_type(http::ContentType::Json);
            return r;
        } catch (const std::exception &e) {
            return http::Response(500, std::string("error: ") + e.what() + "\n");
        }
    }
};

class DbTime : public http::RequestHandler {
public:
    using http::RequestHandler::RequestHandler;
    virtual http::Response handle(const http::RequestIncoming &request) override {
        auto mysql = _app.get_mysql();
        if (!mysql) {
            return http::Response(503, "mysql not initialized\n");
        }
        try {
            auto res = mysql->query_get("SELECT NOW()");
            if (res && res->next()) {
                std::string body = std::string("{\"now\":\"") +
                                   res->getString(1) + "\"}\n";
                http::Response r(200, body);
                r.add_header_content_type(http::ContentType::Json);
                return r;
            }
            http::Response r(500, "{\"now\":null}\n");
            r.add_header_content_type(http::ContentType::Json);
            return r;
        } catch (const std::exception &e) {
            return http::Response(500, std::string("error: ") + e.what() + "\n");
        }
    }
};
#else
// Заглушки, когда MySQL отключён на уровне сборки
class DbHealth : public http::RequestHandler {
public:
    using http::RequestHandler::RequestHandler;
    virtual http::Response handle(const http::RequestIncoming &request) override {
        return http::Response(503, "mysql disabled at build time\n");
    }
};
class DbTime : public http::RequestHandler {
public:
    using http::RequestHandler::RequestHandler;
    virtual http::Response handle(const http::RequestIncoming &request) override {
        return http::Response(503, "mysql disabled at build time\n");
    }
};
#endif


int main(int argc, char const **argv) {
    asio::ssl::context ssl_ctx(asio::ssl::context::tlsv12_server);
    ssl_ctx.use_certificate_chain_file("ssl/cert.pem");
    ssl_ctx.use_private_key_file("ssl/key.pem", asio::ssl::context::pem);
    http::ServerApp app(80, 443, &ssl_ctx);
    
    app.add_endpoint<HttpIndex>("", http::Method::get);
    app.add_endpoint<HttpIndex>("/", http::Method::get);
    app.add_endpoint<HttpIndex>("/index", http::Method::get);
    app.add_endpoint<HttpIndex>("/index.html", http::Method::get);
    // MySQL endpoints
    app.add_endpoint<DbHealth>("/db/health", http::Method::get);
    app.add_endpoint<DbTime>("/db/time", http::Method::get);
    
    app.run(1);
}

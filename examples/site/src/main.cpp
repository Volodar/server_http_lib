#include "http/http.h"


class HttpIndex : public http::RequestHandler
{
public:
    using http::RequestHandler::RequestHandler;
    virtual http::Response handle(const http::Request& request) override{
        std::string body = "Hello world\n";
        body += "\nPath: " + std::string(request.get_path()) + "\n";
        body += "\nParams: \n" + request.get_params().to_string() + "\n";
        body += "\nHeaders: \n" + request.get_headers().to_string('\n') + "\n";
        return http::Response(200, body);
    }
};


int main(int argc, char const **argv) {
    asio::ssl::context ssl_ctx(asio::ssl::context::tlsv12_server);
    ssl_ctx.use_certificate_chain_file("ssl/cert.pem");
    ssl_ctx.use_private_key_file("ssl/key.pem", asio::ssl::context::pem);
    http::ServerApp app(80, 443, &ssl_ctx);
    
    app.add_endpoint<HttpIndex>("", http::Method::get);
    app.add_endpoint<HttpIndex>("/", http::Method::get);
    app.add_endpoint<HttpIndex>("/index", http::Method::get);
    app.add_endpoint<HttpIndex>("/index.html", http::Method::get);
    
    app.run(1);
}

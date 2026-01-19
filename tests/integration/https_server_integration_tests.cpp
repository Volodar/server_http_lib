#include "test_framework.h"
#include "http/http.h"
#include <thread>
#include <chrono>
#include "asio.hpp"
#include "asio/ssl.hpp"
#include <fstream>

static unsigned short kTestHttpsPort = 18443;

TEST(HttpsServer_Status_Get) {
    // Prepare SSL context for server with local certs
    asio::io_context io;
    asio::ssl::context ssl_ctx(asio::ssl::context::tlsv12_server);
    ssl_ctx.set_options(
        asio::ssl::context::default_workarounds |
        asio::ssl::context::no_sslv2 |
        asio::ssl::context::single_dh_use
    );
    // Locate certs under tests_server/ssl for different generators (Xcode/Make)
    auto pick_path = [](std::initializer_list<const char*> candidates) -> std::string {
        for (auto p : candidates) {
            std::ifstream f(p);
            if (f.good()) return std::string(p);
        }
        // Fallback to first (will fail with clear message)
        return std::string(*candidates.begin());
    };
    const std::string cert_path = pick_path({"../ssl/cert.pem", "../../ssl/cert.pem", "ssl/cert.pem"});
    const std::string key_path  = pick_path({"../ssl/key.pem",  "../../ssl/key.pem",  "ssl/key.pem"});
    ssl_ctx.use_certificate_chain_file(cert_path);
    ssl_ctx.use_private_key_file(key_path, asio::ssl::context::pem);

    http::Server server(io, kTestHttpsPort, &ssl_ctx);
    server.add_endpoint("/tls", http::Method::get, [](const http::Request&){
        return http::Response(200, "secure-ok");
    });

    std::thread t([&](){ server.run(1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Simple HTTPS client using ASIO SSL
    asio::ssl::context client_ctx(asio::ssl::context::tlsv12_client);
    client_ctx.set_default_verify_paths();
    asio::ssl::stream<asio::ip::tcp::socket> sock(io, client_ctx);
    // Skip certificate verification for self-signed test certs
    sock.set_verify_mode(asio::ssl::verify_none);

    asio::ip::tcp::resolver resolver(io);
    auto endpoints = resolver.resolve("127.0.0.1", std::to_string(kTestHttpsPort));
    asio::connect(sock.lowest_layer(), endpoints);
    sock.handshake(asio::ssl::stream_base::client);

    std::string req = "GET /tls HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    asio::write(sock, asio::buffer(req));

    asio::streambuf response;
    asio::read_until(sock, response, "\r\n");
    std::istream rs(&response);
    std::string http_version;
    rs >> http_version;
    unsigned int status_code = 0;
    rs >> status_code;
    std::string status_message;
    std::getline(rs, status_message);
    ASSERT_EQ(status_code, 200u);

    // Read headers
    asio::read_until(sock, response, "\r\n\r\n");
    std::string header_line;
    while (std::getline(rs, header_line) && header_line != "\r");

    // Read body
    std::string body;
    if (response.size() > 0) {
        auto bufs = response.data();
        body.append(asio::buffer_cast<const char*>(bufs), response.size());
        response.consume(response.size());
    }
    asio::error_code ec;
    while (asio::read(sock, response, asio::transfer_at_least(1), ec)) {
        auto bufs = response.data();
        body.append(asio::buffer_cast<const char*>(bufs), response.size());
        response.consume(response.size());
    }
    ASSERT_EQ(body, std::string("secure-ok"));

    io.stop();
    if (t.joinable()) t.join();
}

TEST(HttpsServer_Echo_Post) {
    asio::io_context io;
    asio::ssl::context ssl_ctx(asio::ssl::context::tlsv12_server);
    ssl_ctx.set_options(
        asio::ssl::context::default_workarounds |
        asio::ssl::context::no_sslv2 |
        asio::ssl::context::single_dh_use
    );
    auto pick_path = [](std::initializer_list<const char*> candidates) -> std::string {
        for (auto p : candidates) {
            std::ifstream f(p);
            if (f.good()) return std::string(p);
        }
        return std::string(*candidates.begin());
    };
    const std::string cert_path = pick_path({"../ssl/cert.pem", "../../ssl/cert.pem", "ssl/cert.pem"});
    const std::string key_path  = pick_path({"../ssl/key.pem",  "../../ssl/key.pem",  "ssl/key.pem"});
    ssl_ctx.use_certificate_chain_file(cert_path);
    ssl_ctx.use_private_key_file(key_path, asio::ssl::context::pem);

    http::Server server(io, static_cast<unsigned short>(kTestHttpsPort + 1), &ssl_ctx);
    server.add_endpoint("/echo", http::Method::post, [](const http::Request& req){
        return http::Response(200, std::string(req.get_data()));
    });

    std::thread t([&](){ server.run(1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    asio::ssl::context client_ctx(asio::ssl::context::tlsv12_client);
    asio::ssl::stream<asio::ip::tcp::socket> sock(io, client_ctx);
    sock.set_verify_mode(asio::ssl::verify_none);
    asio::ip::tcp::resolver resolver(io);
    auto endpoints = resolver.resolve("127.0.0.1", std::to_string(kTestHttpsPort + 1));
    asio::connect(sock.lowest_layer(), endpoints);
    sock.handshake(asio::ssl::stream_base::client);

    const std::string payload = R"({"tls":true})";
    std::string req;
    req += "POST /echo HTTP/1.1\r\n";
    req += "Host: 127.0.0.1\r\n";
    req += "Content-Type: application/json\r\n";
    req += "Content-Length: ";
    req += std::to_string(payload.size());
    req += "\r\nConnection: close\r\n\r\n";
    req += payload;
    asio::write(sock, asio::buffer(req));

    asio::streambuf response;
    asio::read_until(sock, response, "\r\n");
    std::istream rs(&response);
    std::string http_version;
    rs >> http_version;
    unsigned int status_code = 0;
    rs >> status_code;
    std::string status_message;
    std::getline(rs, status_message);
    ASSERT_EQ(status_code, 200u);

    asio::read_until(sock, response, "\r\n\r\n");
    std::string header_line;
    while (std::getline(rs, header_line) && header_line != "\r");

    std::string body;
    if (response.size() > 0) {
        auto bufs = response.data();
        body.append(asio::buffer_cast<const char*>(bufs), response.size());
        response.consume(response.size());
    }
    asio::error_code ec;
    while (asio::read(sock, response, asio::transfer_at_least(1), ec)) {
        auto bufs = response.data();
        body.append(asio::buffer_cast<const char*>(bufs), response.size());
        response.consume(response.size());
    }
    ASSERT_EQ(body, payload);

    io.stop();
    if (t.joinable()) t.join();
}

#include "test_framework.h"
#include "http/Server.h"
#include "http/http.h"
#include <thread>
#include <chrono>

static unsigned short kPortEp1 = 18081;
static unsigned short kPortEp2 = 18082;

TEST(HttpServer_AddEndpoint_Without_Sequire) {
    asio::io_context io;
    http::Server server(io, kPortEp1);

    server.add_endpoint("/ping", http::Method::get, [](const http::Request& r){
        return http::Response(200, "pong");
    });

    std::thread t([&](){ server.run(1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    http::Url url;
    url.host = "127.0.0.1";
    url.port = kPortEp1;
    url.endpoint = "/ping";
    
    auto r = http::request(url, http::Method::get);
    ASSERT_EQ(r.code, 200);
    ASSERT_EQ(r.body, std::string("pong"));

    io.stop();
    if (t.joinable())
        t.join();
}

TEST(HttpServer_AddEndpoint_With_Sequire) {
    asio::io_context io;
    http::Server server(io, kPortEp2);

    bool guard_called = false;
    server.add_endpoint("/secure", http::Method::get,
        [&](const http::Request&){ return http::Response(200, "ok"); },
        [&](const http::Request&){ guard_called = true; return http::ResponseNone; }
    );

    std::thread t([&](){ server.run(1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    http::Url url; url.host = "127.0.0.1"; url.port = kPortEp2; url.endpoint = "/secure";
    auto r = http::request(url, http::Method::get);
    ASSERT_EQ(r.code, 200);
    ASSERT_EQ(r.body, std::string("ok"));
    ASSERT_TRUE(guard_called);

    io.stop();
    if (t.joinable()) t.join();
}

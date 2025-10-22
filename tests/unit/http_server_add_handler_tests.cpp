#include "test_framework.h"
#include "http/Server.h"
#include "http/http.h"
#include <thread>
#include <chrono>

static unsigned short kPortHandler = 18083;

TEST(HttpServer_AddHandler_Fallback) {
    asio::io_context io;
    http::Server server(io, kPortHandler);

    server.add_handler(http::Method::get, [](const http::Request&){
        return http::Response(200, "fallback");
    });

    std::thread t([&](){ server.run(1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    http::Url url;
    url.host = "127.0.0.1";
    url.port = kPortHandler;
    url.endpoint = "/unknown";
    
    auto r = http::request(url, http::Method::get);
    ASSERT_EQ(r.code, 200);
    ASSERT_EQ(r.body, std::string("fallback"));

    io.stop();
    if (t.joinable()) t.join();
}

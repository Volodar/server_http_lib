#include "test_framework.h"
#include "http/Server.h"
#include "http/http.h"
#include "http/utils.h"
#include <thread>
#include <chrono>

static unsigned short kTestHttpPort = 18080;

TEST(HttpServer_Status_And_Echo) {
    asio::io_context io;
    http::Server server(io, kTestHttpPort);

    server.add_endpoint("/status", http::Method::get, [](const http::Request&){
        return http::Response(200, "Ok");
    });
    server.add_endpoint("/echo", http::Method::post, [](const http::Request& req){
        return http::Response(200, std::string(req.get_data()));
    });

    // Run server in background
    std::thread t([&](){
        server.run(1);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    http::Url url;
    url.host = "127.0.0.1";
    url.port = kTestHttpPort;
    url.endpoint = "/status";
    
    auto r1 = http::request(url, http::Method::get);
    ASSERT_EQ(r1.code, 200);
    ASSERT_EQ(r1.body, std::string("Ok"));

    url.endpoint = "/echo";
    auto payload = std::string("{\"ping\":1}");
    auto r2 = http::request(url, http::Method::post, {http::ContentType::Json, payload});
    ASSERT_EQ(r2.code, 200);
    ASSERT_EQ(r2.body, payload);

    // Stop server and join worker threads via destructor
    io.stop();
    if (t.joinable())
        t.join();
}

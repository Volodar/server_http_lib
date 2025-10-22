#include "test_framework.h"
#include "http/ServerApp.h"
#include "http/http.h"
#include "http/utils.h"
#include <thread>
#include <chrono>
#include <string>

static unsigned short kSrvAppUnitPort1 = 18084;
static unsigned short kSrvAppUnitPort2 = 18085;
static unsigned short kSrvAppUnitPort3 = 18086;

class PongHandler : public http::RequestHandler {
public:
    PongHandler(http::ServerApp& app, std::string msg)
    : http::RequestHandler(app), _msg(std::move(msg)) {}
    http::Response handle(const http::Request&) override {
        return http::Response(200, _msg);
    }
private:
    std::string _msg;
};

class GuardedOkHandler : public http::RequestHandler {
public:
    GuardedOkHandler(http::ServerApp& app, bool& flag)
    : http::RequestHandler(app), _flag(flag) {
        set_sequire([this](const http::Request&){ _flag = true; return http::ResponseNone; });
    }
    http::Response handle(const http::Request&) override { return http::Response(200, "ok"); }
private:
    bool& _flag;
};

class EchoHandler : public http::RequestHandler {
public:
    EchoHandler(http::ServerApp& app):http::RequestHandler(app){}
    http::Response handle(const http::Request& r) override {
        return http::Response(200, std::string(r.get_data()));
    }
};

class DefaultHandler : public http::RequestHandler {
public:
    DefaultHandler(http::ServerApp& app):http::RequestHandler(app){}
    http::Response handle(const http::Request& r) override {
        return http::Response(200, "default");
    }
};

TEST(ServerApp_AddHttpHandler_Without_Sequire) {
    http::ServerApp app(kSrvAppUnitPort1);
    app.add_endpoint<PongHandler>("/ping", http::Method::get, std::string("pong"));

    std::thread t([&](){ app.run(1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    http::Url url{ "127.0.0.1", kSrvAppUnitPort1, "/ping" };
    auto r = http::request(url, http::Method::get);
    ASSERT_EQ(r.code, 200);
    ASSERT_EQ(r.body, std::string("pong"));

    app.get_context().stop();
    if (t.joinable()) t.join();
}

TEST(ServerApp_AddHttpHandler_With_Sequire) {
    http::ServerApp app(kSrvAppUnitPort2);
    bool guard_called = false;
    app.add_endpoint<GuardedOkHandler>("/secure", http::Method::get, std::ref(guard_called));

    std::thread t([&](){ app.run(1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    http::Url url{ "127.0.0.1", kSrvAppUnitPort2, "/secure" };
    auto r = http::request(url, http::Method::get);
    ASSERT_EQ(r.code, 200);
    ASSERT_EQ(r.body, std::string("ok"));
    ASSERT_TRUE(guard_called);

    app.get_context().stop();
    if (t.joinable()) t.join();
}

TEST(ServerApp_Integration_AddHandlers) {
    http::ServerApp app(kSrvAppUnitPort3);
    app.add_endpoint<EchoHandler>("/echo", http::Method::post);
    app.add_handler<DefaultHandler>(http::Method::get);

    std::thread t([&](){ app.run(1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Echo endpoint via POST
    http::Url url{ "127.0.0.1", kSrvAppUnitPort3, "/echo" };
    auto payload = std::string("{\"hello\":1}");
    auto r1 = http::request(url, http::Method::post, {http::ContentType::Json, payload});
    ASSERT_EQ(r1.code, 200);
    ASSERT_EQ(r1.body, payload);

    // Fallback GET handler
    url.endpoint = "/not_found";
    auto r2 = http::request(url, http::Method::get);
    ASSERT_EQ(r2.code, 200);
    ASSERT_EQ(r2.body, std::string("default"));

    app.get_context().stop();
    if (t.joinable()) t.join();
}

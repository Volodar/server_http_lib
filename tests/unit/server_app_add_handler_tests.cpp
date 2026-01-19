#include "test_framework.h"
#include "http/ServerApp.h"
#include "http/http.h"
#include <thread>
#include <chrono>

static unsigned short kSrvAppUnitPort3 = 18086;

class FallbackHandler : public http::RequestHandler {
public:
    using http::RequestHandler::RequestHandler;
    http::Response handle(const http::RequestIncoming&) override { return http::Response(200, "fallback"); }
};

TEST(ServerApp_AddHandler_Fallback) {
    http::ServerApp app(kSrvAppUnitPort3);
    app.add_handler<FallbackHandler>(http::Method::get);

    std::thread t([&](){ app.run(1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    http::Url url{ "127.0.0.1", kSrvAppUnitPort3, "/unknown" };
    auto r = http::request(url, http::Method::get);
    ASSERT_EQ(r.code, 200);
    ASSERT_EQ(r.body, std::string("fallback"));

    app.get_context().stop();
    if (t.joinable()) t.join();
}

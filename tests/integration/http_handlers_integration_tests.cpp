#include "test_framework.h"
#include "http/ServerApp.h"
#include "http/Handlers.h"
#include "http/http.h"
#include <thread>
#include <chrono>
#include <fstream>
#include <filesystem>

static unsigned short kHandlersPort = 18088;

static void write_asset(const std::string& relpath, const std::string& content) {
    std::filesystem::create_directories("assets");
    std::ofstream f("assets/" + relpath, std::ios::binary);
    f << content;
}

TEST(FileContent_Integration_Serves_File) {
    const std::string filename = "integration_test.txt";
    const std::string payload = "integration-body";
    write_asset(filename, payload);

    http::ServerApp app(kHandlersPort);
    app.add_endpoint<http::FileContent>("/" + filename, http::Method::get);

    std::thread t([&](){ app.run(1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    http::Url url{ "127.0.0.1", kHandlersPort, "/" + filename };
    auto r = http::request(url, http::Method::get);
    ASSERT_EQ(r.code, 200);
    ASSERT_EQ(r.body, payload);

    app.get_context().stop();
    if (t.joinable()) t.join();
}

TEST(Redirect_Integration_Returns_301) {
    http::ServerApp app(kHandlersPort + 1);
    app.add_endpoint<http::Redirect>("/go", http::Method::get, std::string("https://example.org"));

    std::thread t([&](){ app.run(1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    http::Url url{ "127.0.0.1", static_cast<int>(kHandlersPort + 1), "/go" };
    auto r = http::request(url, http::Method::get);
    ASSERT_EQ(r.code, 301);
    // Клиент не парсит заголовки, поэтому проверяем только код.

    app.get_context().stop();
    if (t.joinable()) t.join();
}

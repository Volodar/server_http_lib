#include "test_framework.h"
#include "http/Handlers.h"
#include "http/ServerApp.h"
#include "http/RequestOutgoming.h"
#include <fstream>
#include <filesystem>

// Helper to ensure file exists under current working dir
static void write_asset(const std::string& relpath, const std::string& content) {
    std::filesystem::create_directories("assets");
    std::ofstream f("assets/" + relpath, std::ios::binary);
    f << content;
}

TEST(FileContent_Serves_File_And_ContentType) {
    // Prepare asset and request
    const std::string filename = "unit_test.txt";
    const std::string payload = "hello, file";
    write_asset(filename, payload);

    http::ServerApp app(18100);
    http::FileContent handler(app);

    http::RequestOutgoming req;
    std::string path = "/" + filename;
    req.set_path(path);
    auto resp = handler.handle(req);

    ASSERT_EQ(resp.code, 200);
    ASSERT_EQ(resp.body, payload);
    bool has_ct = false;
    for (auto& h : resp.get_headers())
        if (h.first == "Content-Type")
            has_ct = true;
    ASSERT_TRUE(has_ct);
}

TEST(Redirect_Sets_Location_Header) {
    const std::string target = "https://example.com/path";
    http::ServerApp app(18101);
    http::Redirect handler(app, target);

    http::RequestOutgoming req;
    req.set_path("/whatever");
    auto resp = handler.handle(req);

    ASSERT_EQ(resp.code, 301);
    bool has_loc = false;
    for (auto& h : resp.get_headers()){
        if (h.first == "Location" && h.second == target) {
            has_loc = true;
            break;
        }
    }
    ASSERT_TRUE(has_loc);
}

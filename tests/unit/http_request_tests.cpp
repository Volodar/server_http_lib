#include "test_framework.h"
#include "http/Server.h"
#include <string>

TEST(HttpRequest_Parse_And_Params) {
    // Simulated HTTP header
    std::string header;
    header += "POST /api/submit?foo=bar&x=1 HTTP/1.1\r\n";
    header += "Host: 127.0.0.1\r\n";
    header += "User-Agent: TinyTest/1.0\r\n";
    header += "Cookie: token=abc; id=42\r\n";
    header += "Content-Length: 11\r\n\r\n"; // body is provided separately by server, we assign below

    
    http::Request req(header);
    ASSERT_EQ(req.get_method(), http::Method::post);
    ASSERT_EQ(req.get_path(), "/api/submit");
    ASSERT_TRUE(req.get_params().has("foo"));
    ASSERT_TRUE(req.get_params().has("x"));
    ASSERT_EQ(req.get_params().get("foo"), std::string("bar"));
    ASSERT_EQ(req.get_params().get("x"), std::string("1"));

    req.set_data("hello=world");
    ASSERT_EQ(req.get_post_data_param("hello"), std::string("world"));

    ASSERT_EQ(req.get_cookie_value("id"), std::string("42"));
    ASSERT_EQ(req.get_cookie_value("token"), std::string("abc"));
}

TEST(HttpMethod_Conversions) {
    ASSERT_EQ(http::strToMethod("GET"), http::Method::get);
    ASSERT_EQ(http::strToMethod("get"), http::Method::get);
    ASSERT_EQ(http::strToMethod("Post"), http::Method::post);
    ASSERT_EQ(http::strToMethod("post"), http::Method::post);
    ASSERT_EQ(http::methodToStr(http::Method::get), std::string("GET"));
    ASSERT_EQ(http::methodToStr(http::Method::put), std::string("PUT"));
}


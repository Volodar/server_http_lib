#define private public
#include "test_framework.h"
#include "http/Server.h"
#include "http/ssl_session.h"
#undef private

#include <asio.hpp>
#include <string>

using asio::ip::tcp;

namespace {

// Minimal fake socket to exercise do_read error path without networking
struct FakeSocket {
    // Share io_context so the socket is copy/move friendly
    std::shared_ptr<asio::io_context> ctx = std::make_shared<asio::io_context>();
    using executor_type = asio::io_context::executor_type;
    executor_type get_executor() const { return ctx->get_executor(); }

    // lowest_layer to satisfy calls in SslSession
    FakeSocket &lowest_layer() { return *this; }
    const FakeSocket &lowest_layer() const { return *this; }

    // Minimal endpoint/address interface used by SslSession ctor
    struct FakeAddr {
        std::string to_string() const { return std::string("0.0.0.0"); }
    };
    struct FakeEndpoint {
        FakeAddr address() const { return FakeAddr{}; }
    };
    FakeEndpoint remote_endpoint() const { return FakeEndpoint{}; }

    // async_read_some immediately signals EOF to take the error branch
    template <class MutableBufferSequence, class ReadHandler>
    void async_read_some(const MutableBufferSequence &, ReadHandler &&handler) {
        std::error_code ec = asio::error::eof;
        std::forward<ReadHandler>(handler)(ec, static_cast<std::size_t>(0));
    }

    // Provide async_write_some to satisfy asio::async_write used in 100-continue
    template <class ConstBufferSequence, class WriteHandler>
    void async_write_some(const ConstBufferSequence &buffers,
                          WriteHandler &&handler) {
        std::size_t n = 0;
        for (auto it = asio::buffer_sequence_begin(buffers);
             it != asio::buffer_sequence_end(buffers); ++it) {
            n += it->size();
        }
        std::error_code ec; // success
        std::forward<WriteHandler>(handler)(ec, n);
    }

    // No-op shutdown/close to satisfy potential calls
    void shutdown(asio::ip::tcp::socket::shutdown_type, std::error_code &) {}
    void close(std::error_code &) {}
};

// Helper to build a fresh SslSession with tcp::socket
static std::shared_ptr<http::SslSession<tcp::socket>>
make_tcp_session(asio::io_context &io, const http::EndpointMap &endpoints,
                 const std::map<http::Method, http::Handler> &handlers) {
    tcp::socket sock(io);
    auto s = std::make_shared<http::SslSession<tcp::socket>>(std::move(sock), &endpoints, &handlers);
    return s;
}

} // namespace

TEST(SslSession_ci_starts_with_CaseInsensitivity_And_Bounds) {
    using Sess = http::SslSession<tcp::socket>;
    const char *s1 = "Content-Length: 12";
    ASSERT_TRUE(Sess::ci_starts_with(s1, std::strlen(s1), "content-length:"));
    ASSERT_TRUE(Sess::ci_starts_with("EXPECT: 100-continue", 21, "Expect:"));
    ASSERT_FALSE(Sess::ci_starts_with("Con", 3, "Content-Length:")); // shorter than literal
    ASSERT_FALSE(Sess::ci_starts_with("X-Content-Length:", 17, "Content-Length:"));
}

// Provide template specializations for FakeSocket to satisfy references from
// header-defined methods (try_parse_and_handle/do_read) in this TU.
namespace http {
template <>
void SslSession<FakeSocket>::process_request(const std::string& , const std::string& ) {
    // no-op for FakeSocket path; not used in assertions in this file
}
template <>
void SslSession<FakeSocket>::do_write() {
    // no-op for FakeSocket; real sockets have specializations in library
}
} // namespace http

TEST(SslSession_find_header_end_Boundaries_And_Detection) {
    asio::io_context io;
    http::EndpointMap endpoints;
    std::map<http::Method, http::Handler> handlers;
    auto s = make_tcp_session(io, endpoints, handlers);

    // Too short
    s->_buf.assign({'G','E','T'});
    s->_used = s->_buf.size();
    ASSERT_EQ(s->find_header_end(), std::string::npos);

    // Only LFs, no CRLFCRLF
    s->_buf.assign({'G','E','T',' ','/',' ','H','T','T','P','/','1','.','1','\n','H','o','s','t',':','x','\n','\n'});
    s->_used = s->_buf.size();
    ASSERT_EQ(s->find_header_end(), std::string::npos);

    // Proper CRLFCRLF
    std::string hdr = "GET / HTTP/1.1\r\nHost: x\r\n\r\n";
    s->_buf.assign(hdr.begin(), hdr.end());
    s->_used = s->_buf.size();
    size_t pos = s->find_header_end();
    ASSERT_NE(pos, std::string::npos);
    ASSERT_EQ(pos + 1, hdr.size()); // end points to last \n
    // With extra data after headers — still finds first boundary
    std::string pipelined = hdr + "EXTRA";
    s->_buf.assign(pipelined.begin(), pipelined.end());
    s->_used = s->_buf.size();
    size_t pos2 = s->find_header_end();
    ASSERT_EQ(pos2 + 1, hdr.size());
}

TEST(SslSession_try_parse_and_handle_IncompleteHeader_And_PartialBody) {
    asio::io_context io;
    http::EndpointMap endpoints;
    std::map<http::Method, http::Handler> handlers;
    auto s = make_tcp_session(io, endpoints, handlers);

    // Incomplete header — no CRLFCRLF
    std::string h1 = "GET /a HTTP/1.1\r\nHost: x\r\n";
    s->_buf.assign(h1.begin(), h1.end());
    s->_used = s->_buf.size();
    ASSERT_FALSE(s->try_parse_and_handle());
    ASSERT_FALSE(s->_have_headers);

    // Complete header with Content-Length, but body not fully received
    std::string h2 = "POST /b HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\n\r\n";
    std::string partial = h2 + "ab"; // only 2 of 5 bytes
    s->_buf.assign(partial.begin(), partial.end());
    s->_used = s->_buf.size();
    ASSERT_FALSE(s->try_parse_and_handle());
    ASSERT_TRUE(s->_have_headers);
    ASSERT_EQ(s->_content_length, 5u);
    ASSERT_NE(s->_headers_len, 0u);
}

TEST(SslSession_try_parse_and_handle_Expect100Continue_EarlyResponds) {
    asio::io_context io;
    http::EndpointMap endpoints;
    std::map<http::Method, http::Handler> handlers;
    auto s = make_tcp_session(io, endpoints, handlers);

    std::string h =
        "POST /upload HTTP/1.1\r\nHost: x\r\nExpect: 100-Continue\r\nContent-Length: 4\r\n\r\n";
    s->_buf.assign(h.begin(), h.end());
    s->_used = s->_buf.size();
    // Should schedule interim 100 and return true without parsing body
    ASSERT_TRUE(s->try_parse_and_handle());
    ASSERT_TRUE(s->_have_headers);
    ASSERT_EQ(s->_content_length, 4u);
}

TEST(SslSession_process_request_Routes_KeepAlive_And_Fallbacks) {
    asio::io_context io;

    // Handlers and endpoints
    http::EndpointMap endpoints;
    std::map<http::Method, http::Handler> handlers;

    // Endpoint handler and sequire
    bool main_called = false;
    bool sequire_called = false;
    endpoints[{"/x", http::Method::get}] = std::make_pair(
        [&](const http::Request &) {
            main_called = true;
            http::Response r{200, "Hello"};
            r.add_header("X-Test: A");
            return r;
        },
        [&](const http::Request &) {
            sequire_called = true;
            return http::ResponseNone; // allow main to run
        });

    // Fallback method handler
    handlers[http::Method::get] = [&](const http::Request &) {
        return http::Response{201, "Meth"};
    };

    auto s = make_tcp_session(io, endpoints, handlers);

    // 1) Endpoint hit, HTTP/1.1 -> keep-alive
    std::string hdr1 = "GET /x HTTP/1.1\r\nHost: h\r\n\r\n";
    s->process_request(hdr1, "");
    ASSERT_TRUE(main_called);
    ASSERT_TRUE(sequire_called);
    ASSERT_TRUE(s->_keep_alive);
    ASSERT_NE(s->_http_header.find("HTTP/1.1 200 OK\r\n"), std::string::npos);
    ASSERT_NE(s->_http_header.find("Content-Length: 5\r\n"), std::string::npos);
    ASSERT_NE(s->_http_header.find("Connection: keep-alive\r\n"), std::string::npos);
    ASSERT_NE(s->_http_header.find("Keep-Alive: timeout=10\r\n\r\n"), std::string::npos);
    ASSERT_EQ(s->_http_body, std::string("Hello"));

    // 2) Header forces close
    std::string hdr2 =
        "GET /x HTTP/1.1\r\nHost: h\r\nConnection: close\r\n\r\n";
    s->process_request(hdr2, "");
    ASSERT_FALSE(s->_keep_alive);
    ASSERT_NE(s->_http_header.find("Connection: close\r\n\r\n"), std::string::npos);

    // 3) No endpoint, fallback method handler, HTTP/1.0 -> close by default
    std::string hdr3 = "GET /nope HTTP/1.0\r\nHost: h\r\n\r\n";
    s->process_request(hdr3, "");
    ASSERT_NE(s->_http_header.find("HTTP/1.1 201 OK\r\n"), std::string::npos);
    ASSERT_FALSE(s->_keep_alive);
}

TEST(SslSession_process_request_SequireBlocksWith403) {
    asio::io_context io;
    http::EndpointMap endpoints;
    std::map<http::Method, http::Handler> handlers;

    bool main_called = false;
    endpoints[{"/secure", http::Method::get}] = std::make_pair(
        [&](const http::Request &) {
            main_called = true;
            return http::Response{200, "OK"};
        },
        [&](const http::Request &) { return http::Response403; });

    auto s = make_tcp_session(io, endpoints, handlers);
    std::string hdr = "GET /secure HTTP/1.1\r\nHost: h\r\n\r\n";
    s->process_request(hdr, "");
    ASSERT_FALSE(main_called);
    ASSERT_NE(s->_http_header.find("HTTP/1.1 403 OK\r\n"), std::string::npos);
}

TEST(SslSession_do_read_ErrorEOF_NoCrash_And_BufferPrepared) {
    // Construct session with FakeSocket to force EOF on read
    http::EndpointMap endpoints;
    std::map<http::Method, http::Handler> handlers;
    auto sess = std::make_shared<http::SslSession<FakeSocket>>(FakeSocket{}, &endpoints, &handlers);

    // Call do_read() directly; ensure buffer grew and no data used
    sess->do_read();
    ASSERT_TRUE(sess->_buf.size() >= 8192);
    ASSERT_EQ(sess->_used, 0u);
}

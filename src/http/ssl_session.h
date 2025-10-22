//
//  ssl_session.m
//  server_web_tests
//
//  Created by Vladimir Tolmachev on 21.10.2025.
//
#ifndef SSH_SESSION_H
#define SSH_SESSION_H

#include "asio.hpp"
#include "asio/ssl.hpp"
#include "http_common.h"
#include <chrono>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace http {

struct EndpointKey {
    std::string path;
    Method method;
    bool operator==(const EndpointKey &other) const {
        return path == other.path && method == other.method;
    }
};

struct EndpointKeyHash {
    std::size_t operator()(const EndpointKey &k) const {
        return std::hash<std::string>()(k.path) ^
               (std::hash<int>()(static_cast<int>(k.method)) << 1);
    }
};

using Handler = std::function<Response(const Request &)>;
using EndpointMap = std::unordered_map<EndpointKey, std::pair<Handler, Handler>,
                                       EndpointKeyHash>;

template <typename SocketType>
class SslSession : public std::enable_shared_from_this<SslSession<SocketType>> {
  public:
    SslSession(SocketType socket, const EndpointMap *endpoints, const std::map<Method, Handler> *handlers)
    : _socket(std::move(socket))
    , _endpoints(endpoints)
    , _handlers(handlers)
    , _ka_timer(_socket.get_executor()) {
        try {
            auto remote = _socket.lowest_layer().remote_endpoint();
            _clientIP = remote.address().to_string();
        } catch (const std::exception &e) {
            std::cerr << "Exception on create session: " << e.what()
                      << std::endl;
        }
        // Pre-reserve buffers to reduce reallocations
        _http_header.reserve(512);
        _http_body.reserve(1024);
        _buf.resize(8192);
    }
    void start() {
        do_handshake_or_read();
    }

  private:
    void do_handshake_or_read();
    void process_request(const std::string &header, const std::string &body);

    void do_read() {
        auto self = this->shared_from_this();
        ensure_capacity();
        _socket.async_read_some(
            asio::buffer(_buf.data() + _used, _buf.size() - _used),
            [this, self](std::error_code ec, std::size_t n) {
                if (ec) {
                    using asio::error::eof;
                    using asio::error::connection_reset;
                    using asio::error::operation_aborted;
                    using asio::error::not_connected;
                    using asio::error::connection_aborted;
#if defined(ASIO_HAS_SSL)
                    if (ec == asio::ssl::error::stream_truncated)
                        return;
#endif
                    if (ec == eof || ec == connection_reset ||
                        ec == operation_aborted || ec == not_connected ||
                        ec == connection_aborted) {
                        return; // normal closure
                    }
                    std::cerr << "Http read error: " << ec.message()
                              << std::endl;
                    return;
                }
                cancel_keep_alive_timeout();
                _used += n;
                if (try_parse_and_handle()) {
                    return; // handled; do_write will re-arm on keep-alive
                }
                do_read();
            });
    }
    void ensure_capacity() {
        if (_buf.empty())
            _buf.resize(8192);
        if (_used == _buf.size())
            _buf.resize(_buf.size() * 2);
    }
    size_t find_header_end() const {
        if (_used < 4)
            return std::string::npos;
        for (size_t i = 3; i < _used; ++i) {
            if (_buf[i - 3] == '\r' && _buf[i - 2] == '\n' &&
                _buf[i - 1] == '\r' && _buf[i] == '\n')
                return i;
        }
        return std::string::npos;
    }
    static bool ci_starts_with(const char *s, size_t n, const char *lit) {
        size_t m = std::strlen(lit);
        if (n < m)
            return false;
        for (size_t i = 0; i < m; ++i) {
            char a = s[i];
            if (a >= 'A' && a <= 'Z')
                a += 32;
            char b = lit[i];
            if (b >= 'A' && b <= 'Z')
                b += 32;
            if (a != b)
                return false;
        }
        return true;
    }
    bool try_parse_and_handle() {
        if (!_have_headers) {
            size_t end = find_header_end();
            if (end == std::string::npos)
                return false;
            _headers_len = end + 1; // include last \n
            // Scan headers
            const char *p = _buf.data();
            const char *stop = _buf.data() + _headers_len;
            _content_length = 0;
            _expect_continue = false;
            // Skip request line
            const char *line_end =
                static_cast<const char *>(memchr(p, '\n', stop - p));
            if (!line_end)
                return false;
            p = line_end + 1;
            while (p < stop) {
                const char *e =
                    static_cast<const char *>(memchr(p, '\n', stop - p));
                if (!e)
                    break;
                const char *ls = p;
                const char *le = (e > p && *(e - 1) == '\r') ? e - 1 : e;
                if (le == ls) {
                    p = e + 1;
                    continue;
                }
                size_t len = static_cast<size_t>(le - ls);
                if (ci_starts_with(ls, len, "Content-Length:")) {
                    const char *v = ls + std::strlen("Content-Length:");
                    while (v < le && (*v == ' ' || *v == '\t'))
                        ++v;
                    size_t val = 0;
                    while (v < le && *v >= '0' && *v <= '9') {
                        val = val * 10 + (*v - '0');
                        ++v;
                    }
                    _content_length = val;
                } else if (ci_starts_with(ls, len, "Expect:")) {
                    const char *v = ls + std::strlen("Expect:");
                    while (v < le && (*v == ' ' || *v == '\t'))
                        ++v;
                    static const char cont[] = "100-continue";
                    size_t m = sizeof(cont) - 1;
                    bool match = (static_cast<size_t>(le - v) >= m);
                    if (match) {
                        for (size_t i = 0; i < m; ++i) {
                            char a = v[i];
                            if (a >= 'A' && a <= 'Z')
                                a += 32;
                            if (a != cont[i]) {
                                match = false;
                                break;
                            }
                        }
                    }
                    if (match)
                        _expect_continue = true;
                }
                p = e + 1;
            }
            _have_headers = true;
            if (_expect_continue) {
                auto self = this->shared_from_this();
                static const char interim[] = "HTTP/1.1 100 Continue\r\n\r\n";
                asio::async_write(
                    _socket, asio::buffer(interim, sizeof(interim) - 1),
                    [this, self](std::error_code, std::size_t) { do_read(); });
                return true;
            }
        }
        if (_used < _headers_len + _content_length)
            return false;

        std::string header_str(_buf.data(), _headers_len);
        std::string body_str;
        if (_content_length)
            body_str.assign(_buf.data() + _headers_len, _content_length);

        size_t consumed = _headers_len + _content_length;
        size_t left = _used - consumed;
        if (left > 0)
            std::memmove(_buf.data(), _buf.data() + consumed, left);
        _used = left;
        _have_headers = false;
        _headers_len = 0;
        _content_length = 0;
        _expect_continue = false;

        process_request(header_str, body_str);
        return true;
    }

    void do_write();

    void arm_keep_alive_timeout() {
        if (!_keep_alive)
            return;
        std::error_code ec;
        _ka_timer.expires_after(std::chrono::seconds(_keep_alive_timeout));
        auto self = this->shared_from_this();
        _ka_timer.async_wait([this, self](const std::error_code &e) {
            if (!e) {
                std::error_code ec2;
                // Timed out: close connection
                _socket.lowest_layer().shutdown(
                    asio::ip::tcp::socket::shutdown_both, ec2);
                _socket.lowest_layer().close(ec2);
            }
        });
    }
    void cancel_keep_alive_timeout() {
        std::error_code ec;
        _ka_timer.cancel(ec);
    }

  private:
    SocketType _socket;
    std::vector<char> _buf;
    size_t _used = 0;
    std::string _http_header;
    std::string _http_body;
    const EndpointMap *_endpoints;
    const std::map<Method, Handler> *_handlers;
    std::string _clientIP;
    bool _keep_alive = false;
    int _keep_alive_timeout = 10; // seconds
    asio::steady_timer _ka_timer;
    bool _have_headers = false;
    size_t _headers_len = 0;
    size_t _content_length = 0;
    bool _expect_continue = false;
};

} // namespace http

#endif

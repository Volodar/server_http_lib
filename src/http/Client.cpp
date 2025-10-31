#include "http/Client.h"
#include "asio.hpp"
#include "asio/ssl.hpp"
#include <array>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <string>

#include <sys/socket.h>
#include <sys/time.h>

namespace http {

template <typename SocketLike>
static void apply_timeouts(SocketLike &sock, const RequestOptions &opt) {
  int fd = sock.native_handle();
  if (fd < 0)
    return;
    if (opt.read_timeout_ms > 0) {
        timeval tv{};
        tv.tv_sec = opt.read_timeout_ms / 1000;
        tv.tv_usec = (opt.read_timeout_ms % 1000) * 1000;
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }
    if (opt.connect_timeout_ms > 0 || opt.read_timeout_ms > 0) {
        int ms = opt.connect_timeout_ms > 0 ? opt.connect_timeout_ms
                                            : opt.read_timeout_ms;
        timeval tv{};
        tv.tv_sec = ms / 1000;
        tv.tv_usec = (ms % 1000) * 1000;
        ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    }
}

static Response do_request(const std::string& host, int port,
                           const std::string& path, const std::string& method,
                           const std::string& content_type,
                           const std::string& body, const RequestOptions &opt) {
    Response result{-1, ""};
    try {
        asio::io_service io;
        asio::ip::tcp::resolver resolver(io);
        asio::ip::tcp::resolver::query query(host, std::to_string(port));
        auto endpoints = resolver.resolve(query);

        // Build HTTP request
        std::string req;
        const bool has_body = !body.empty();
        req.reserve(64 + path.size() + host.size() +
                    (has_body ? (64 + body.size()) : 0));
        req += method;
        req += ' ';
        req += path;
        req += " HTTP/1.1\r\n";
        req += "Host: ";
        req += host;
        req += "\r\n";
        for (auto &h : opt.headers) {
            if(h.first == "Content-Type" || h.first == "Content-Length")
                continue;
            req += h.first;
            req += ": ";
            req += h.second;
            req += "\r\n";
        }
        if (has_body) {
            req += "Content-Type: ";
            req += content_type;
            req += "\r\n";
            req += "Content-Length: ";
            req += std::to_string(body.size());
            req += "\r\n";
        }
        req += "Connection: close\r\n\r\n";
        if (has_body)
            req.append(body);

        if (!opt.https) {
            asio::ip::tcp::socket socket(io);
            asio::connect(socket, endpoints);
            apply_timeouts(socket, opt);
            asio::write(socket, asio::buffer(req));

            // Read response
            std::string header;
            header.reserve(1024);
            std::array<char, 4096> buf;
            std::size_t header_end = std::string::npos;
            asio::error_code ec;
            std::string leftover;
            while (true) {
                std::size_t n =
                    socket.read_some(asio::buffer(buf.data(), buf.size()), ec);
                if (ec && ec != asio::error::eof)
                    break;
                header.append(buf.data(), n);
                auto pos = header.find("\r\n\r\n");
                if (pos != std::string::npos) {
                    header_end = pos + 4;
                    break;
                }
                if (ec == asio::error::eof)
                    break;
            }
            if (header_end == std::string::npos)
                return result;
            // Parse status code
            std::size_t sp1 = header.find(' ');
            std::size_t sp2 = (sp1 == std::string::npos)
                                  ? std::string::npos
                                  : header.find(' ', sp1 + 1);
            unsigned code = 0;
            if (sp1 != std::string::npos && sp2 != std::string::npos) {
                for (std::size_t i = sp1 + 1; i < sp2; ++i) {
                    char c = header[i];
                    if (c >= '0' && c <= '9')
                        code = code * 10 + (c - '0');
                    else
                        break;
                }
            }
            result.code = code;
            // Content-Length
            std::size_t content_length = 0;
            std::size_t line_start = header.find("\r\n");
            if (line_start != std::string::npos)
                line_start += 2;
            while (line_start < header_end) {
                std::size_t line_end = header.find("\r\n", line_start);
                if (line_end == std::string::npos || line_end > header_end)
                    break;
                std::size_t len = line_end - line_start;
                const char *ls = header.data() + line_start;
                static const char kcl[] = "content-length:";
                bool is_cl = len >= sizeof(kcl) - 1;
                if (is_cl) {
                    for (size_t i = 0; i < sizeof(kcl) - 1; ++i) {
                        char a = ls[i];
                        if (a >= 'A' && a <= 'Z')
                            a += 32;
                        if (a != kcl[i]) {
                            is_cl = false;
                            break;
                        }
                    }
                }
                if (is_cl) {
                    std::size_t i = sizeof(kcl) - 1;
                    while (i < len && (ls[i] == ' ' || ls[i] == '\t'))
                        ++i;
                    std::size_t val = 0;
                    while (i < len && ls[i] >= '0' && ls[i] <= '9') {
                        val = val * 10 + (ls[i] - '0');
                        ++i;
                    }
                    content_length = val;
                }
                line_start = line_end + 2;
            }
            if (header.size() > header_end) {
                leftover.assign(header.data() + header_end,
                                header.size() - header_end);
            }
            header.clear();
            if (content_length > 0) {
                result.body.reserve(content_length);
                if (!leftover.empty()) {
                    result.body.append(
                        leftover.data(),
                        std::min(leftover.size(), content_length));
                }
                while (result.body.size() < content_length) {
                    asio::error_code ec2;
                    std::size_t n = socket.read_some(
                        asio::buffer(buf.data(), buf.size()), ec2);
                    if (n > 0) {
                        std::size_t need = content_length - result.body.size();
                        result.body.append(buf.data(), std::min(need, n));
                    }
                    if (ec2)
                        break;
                }
            } else {
                if (!leftover.empty())
                    result.body.append(leftover);
                for (;;) {
                    asio::error_code ec2;
                    std::size_t n = socket.read_some(asio::buffer(buf), ec2);
                    if (n > 0)
                        result.body.append(buf.data(), n);
                    if (ec2)
                        break;
                }
            }
        } else {
            asio::ssl::context ctx(asio::ssl::context::tlsv12_client);
            asio::ssl::stream<asio::ip::tcp::socket> socket(io, ctx);
            socket.set_verify_mode(asio::ssl::verify_none);
            asio::connect(socket.lowest_layer(), endpoints);
            apply_timeouts(socket.lowest_layer(), opt);
            socket.handshake(asio::ssl::stream_base::client);
            asio::write(socket, asio::buffer(req));

            std::string header;
            header.reserve(1024);
            std::array<char, 4096> buf;
            std::size_t header_end = std::string::npos;
            asio::error_code ec;
            std::string leftover;
            while (true) {
                std::size_t n =
                    socket.read_some(asio::buffer(buf.data(), buf.size()), ec);
                if (ec && ec != asio::error::eof)
                    break;
                header.append(buf.data(), n);
                auto pos = header.find("\r\n\r\n");
                if (pos != std::string::npos) {
                    header_end = pos + 4;
                    break;
                }
                if (ec == asio::error::eof)
                    break;
            }
            if (header_end == std::string::npos)
                return result;
            std::size_t sp1 = header.find(' ');
            std::size_t sp2 = (sp1 == std::string::npos)
                                  ? std::string::npos
                                  : header.find(' ', sp1 + 1);
            unsigned code = 0;
            if (sp1 != std::string::npos && sp2 != std::string::npos) {
                for (std::size_t i = sp1 + 1; i < sp2; ++i) {
                    char c = header[i];
                    if (c >= '0' && c <= '9')
                        code = code * 10 + (c - '0');
                    else
                        break;
                }
            }
            result.code = code;
            std::size_t content_length = 0;
            std::size_t line_start = header.find("\r\n");
            if (line_start != std::string::npos)
                line_start += 2;
            while (line_start < header_end) {
                std::size_t line_end = header.find("\r\n", line_start);
                if (line_end == std::string::npos || line_end > header_end)
                    break;
                std::size_t len = line_end - line_start;
                const char *ls = header.data() + line_start;
                static const char kcl[] = "content-length:";
                bool is_cl = len >= sizeof(kcl) - 1;
                if (is_cl) {
                    for (size_t i = 0; i < sizeof(kcl) - 1; ++i) {
                        char a = ls[i];
                        if (a >= 'A' && a <= 'Z')
                            a += 32;
                        if (a != kcl[i]) {
                            is_cl = false;
                            break;
                        }
                    }
                }
                if (is_cl) {
                    std::size_t i = sizeof(kcl) - 1;
                    while (i < len && (ls[i] == ' ' || ls[i] == '\t'))
                        ++i;
                    std::size_t val = 0;
                    while (i < len && ls[i] >= '0' && ls[i] <= '9') {
                        val = val * 10 + (ls[i] - '0');
                        ++i;
                    }
                    content_length = val;
                }
                line_start = line_end + 2;
            }
            if (header.size() > header_end) {
                leftover.assign(header.data() + header_end,
                                header.size() - header_end);
            }
            header.clear();
            if (content_length > 0) {
                result.body.reserve(content_length);
                if (!leftover.empty()) {
                    result.body.append(
                        leftover.data(),
                        std::min(leftover.size(), content_length));
                }
                while (result.body.size() < content_length) {
                    asio::error_code ec2;
                    std::size_t n = socket.read_some(
                        asio::buffer(buf.data(), buf.size()), ec2);
                    if (n > 0) {
                        std::size_t need = content_length - result.body.size();
                        result.body.append(buf.data(), std::min(need, n));
                    }
                    if (ec2)
                        break;
                }
            } else {
                if (!leftover.empty())
                    result.body.append(leftover);
                for (;;) {
                    asio::error_code ec2;
                    std::size_t n = socket.read_some(asio::buffer(buf), ec2);
                    if (n > 0)
                        result.body.append(buf.data(), n);
                    if (ec2)
                        break;
                }
            }
        }
    } catch (...) {
    }
    return result;
}

Response Client::get(const std::string& host, int port, const std::string& path,
                     const RequestOptions &opt) {
    return do_request(host, port, path, "GET", "", "", opt);
}

Response Client::post(const std::string& host, int port,
                      const std::string& path, const std::string& content_type,
                      const std::string& body, const RequestOptions &opt) {
    return do_request(host, port, path, "POST", content_type, body, opt);
}

} // namespace http

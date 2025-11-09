#include "ssl_session.h"
#include "http.h"
#include <charconv>
#include "Log.h"

namespace http {

template <typename SocketType>
void SslSession<SocketType>::process_request(std::string&& header_,
                                             std::string&& body_) {
    Request request(std::move(header_));
    request.set_user_ip(_clientIP);
    request.set_data(std::move(body_));

    EndpointKey key{std::string(request.get_path()), request.get_method()};
    Response response = Response404;
    std::string statusText = "OK";
    auto it = _endpoints->find(key);
    if (it != _endpoints->end()) {
        try {
            auto sequire = it->second.second;
            if (sequire)
                response = sequire(request);
            if (response.code == ResponseNone.code || response.code == Response404.code)
                response = it->second.first(request);
        } catch (const std::exception &e) {
            log_error << e.what();
        }
    } else {
        auto ith = _handlers->find(request.get_method());
        if (ith != _handlers->end()) {
            try {
                response = ith->second(request);
            } catch (const std::exception &e) {
                log_error << e.what();
            }
        }
    }

    // Determine keep-alive from headers and HTTP version
    auto has_ci = [](const std::string& s, const char *needle) -> bool {
        size_t n = std::strlen(needle);
        auto it = std::search(
            s.begin(), s.end(), needle, needle + n,
            [](char a, char b) { return std::tolower(a) == std::tolower(b); });
        return it != s.end();
    };
    
    const std::string& header = request.get_source_header_string();
    bool http11 = header.find("HTTP/1.1") != std::string::npos;
    bool conn_close = has_ci(header, "Connection: close");
    bool conn_keep = has_ci(header, "Connection: keep-alive");
    _keep_alive = (http11 && !conn_close) || conn_keep;

    // Формируем HTTP-ответ без ostringstream и без копирования body
    // Steal response body to avoid copy
    _http_body.clear();
    _http_body.swap(response.body);
    _http_header.clear();
    // Reserve: status line + headers + content-length + CRLFs (keep capacity
    // across requests)
    if (_http_header.capacity() < 512)
        _http_header.reserve(512);
    _http_header += "HTTP/1.1 ";
    {
        char num[16];
        auto res = std::to_chars(std::begin(num), std::end(num), response.code);
        _http_header.append(num, res.ptr);
    }
    _http_header += " ";
    _http_header += statusText;
    _http_header += "\r\n";
    for (auto &h : response.headers) {
        _http_header += h;
        _http_header += "\r\n";
    }
    _http_header += "Content-Length: ";
    {
        char num[32];
        auto res =
            std::to_chars(std::begin(num), std::end(num), _http_body.size());
        _http_header.append(num, res.ptr);
    }
    if (_keep_alive) {
        _http_header += "\r\nConnection: keep-alive\r\n";
        _http_header += "Keep-Alive: timeout=";
        _http_header += std::to_string(_keep_alive_timeout);
        _http_header += "\r\n\r\n";
    } else {
        _http_header += "\r\nConnection: close\r\n\r\n";
    }

    do_write();
}

template <>
void SslSession<asio::ssl::stream<asio::ip::tcp::socket>>::do_write() {
    auto self = this->shared_from_this();
    std::array<asio::const_buffer, 2> bufs{asio::buffer(_http_header),
                                           asio::buffer(_http_body)};
    asio::async_write(_socket, bufs,
                      [this, self](std::error_code ec, std::size_t /*length*/) {
                          if (!_keep_alive) {
                              _socket.shutdown(ec);
                          } else {
                              // Re-arm read for next request and start idle
                              // timer
                              cancel_keep_alive_timeout();
                              arm_keep_alive_timeout();
                              do_read();
                          }
                      });
}

template <> void SslSession<asio::ip::tcp::socket>::do_write() {
    auto self = this->shared_from_this();
    std::array<asio::const_buffer, 2> bufs{asio::buffer(_http_header),
                                           asio::buffer(_http_body)};
    asio::async_write(_socket, bufs,
                      [this, self](std::error_code ec, std::size_t /*length*/) {
                          if (!_keep_alive) {
                              _socket.shutdown(
                                  asio::ip::tcp::socket::shutdown_both, ec);
                          } else {
                              cancel_keep_alive_timeout();
                              arm_keep_alive_timeout();
                              do_read();
                          }
                      });
}

} // namespace http

// Explicit instantiations to make template definitions linkable from other TUs
template void http::SslSession<asio::ip::tcp::socket>::process_request(
    std::string&& , std::string&& );
#if defined(ASIO_HAS_SSL)
template void http::SslSession<asio::ssl::stream<asio::ip::tcp::socket>>::process_request(
    std::string&& , std::string&& );
#endif

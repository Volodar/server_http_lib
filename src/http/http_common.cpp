#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <list>
#include <charconv>
#include "asio.hpp"
#include "asio/ssl.hpp"
#include "utils.h"
#include "Log.h"
#include "Request.h"
#include "RequestOutgoming.h"
#include "http_common.h"

namespace http {
const std::string string_empty;
const std::string CONTENT_TYPE("Content-Type");

Response ResponseNone{0, ""};
Response ResponseOk{200, "Ok"};
Response Response403{403, "403 Forbidden"};
Response Response404{404, "404 Not Found"};

bool Params::has(std::string_view name) const {
    return _params.count(name) > 0;
}
template <> std::string Params::get(std::string_view name) const = delete;
template <> int Params::get(std::string_view name) const {
    return std::stoi(std::string(get(name)));
}
template <> long Params::get(std::string_view name) const {
    return std::stol(std::string(get(name)));
}
template <> long long Params::get(std::string_view name) const {
    return std::stoll(std::string(get(name)));
}
template <> float Params::get(std::string_view name) const {
    return std::stof(std::string(get(name)));
}
std::string_view Params::get(std::string_view name) const {
    auto iter = _params.find(name);
    if (iter != _params.end())
        return iter->second;
    return string_empty;
}
void Params::set(std::string_view name, std::string_view value) {
    _len += name.size() + value.size() + 1;
    _params.insert_or_assign(name, value);
}
bool Params::empty() const { return _params.empty(); }
size_t Params::size() const { return _params.size(); }
std::string Params::to_string(char delimiter) const {
    std::string res;
    res.reserve(_len + _params.size());
    size_t index = _params.size();
    for (auto &pair : _params) {
        auto append = std::string(pair.first) + "=" + std::string(pair.second);
        assert(res.capacity() >= res.size() + append.size());
        res += std::move(append);
        if (--index > 0){
            assert(res.capacity() > res.size());
            res += delimiter;
        }
    }
    return res;
}

static inline char ascii_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c | 0x20) : c;
}
static inline bool is_equal_lower(const char* a, size_t a_size, const std::string_view& b_in_lower){
    if(a_size != b_in_lower.size())
        return false;
    for(size_t i=0; i<a_size; ++i){
        if(ascii_lower(a[i]) != b_in_lower[i])
            return false;
    }
    return true;
}
Method strToMethod(const std::string& methodStr) {
    const char *s = methodStr.c_str();
    size_t size = methodStr.size();
    if(is_equal_lower(s, size, "get")) return Method::get;
    if(is_equal_lower(s, size, "post")) return Method::post;
    if(is_equal_lower(s, size, "put")) return Method::put;
    if(is_equal_lower(s, size, "delete")) return Method::del;
    if(is_equal_lower(s, size, "create")) return Method::create;
    return Method::get;
}
std::string methodToStr(Method method) {
    if (method == Method::get)
        return "GET";
    else if (method == Method::post)
        return "POST";
    else if (method == Method::put)
        return "PUT";
    else if (method == Method::del)
        return "DELETE";
    else if (method == Method::create)
        return "CREATE";
    else
        return "GET";
}

std::ostream& operator<<(std::ostream& os, Method method) {
    os << methodToStr(method);
    return os;
}

Response::Response()
: code(-1) {
}
Response::Response(int code, const std::string& body) {
    this->code = code;
    this->body = body;
}
void Response::add_header(const std::string& name, const std::string& value) {
    if(name == "Content-Length")
        return;
    _headers.push_back({name, value});
    _headers_buffer_len += name.size() + value.size();
}
void Response::add_header_content_type(const std::string& type) {
    add_header(CONTENT_TYPE, type);
}

std::string Response::get_http_header(bool keep_alive, size_t keep_alive_timeout) const {
    std::string header;
    std::string code = std::to_string(this->code);
    std::string content_len = std::to_string(body.size());

    const_cast<Response*>(this)->_headers.push_back({"Content-Length", std::to_string(body.size())});
    std::string headers;
    headers.reserve(_headers_buffer_len + _headers.size() * 4 + 14 + _headers.back().second.size());
    for (auto &pair : _headers) {
        headers += pair.first + ": " + pair.second + "\r\n";
    }
    
    std::string footer;
    if (keep_alive) {
        footer.reserve(64);
        footer += "Connection: keep-alive\r\nKeep-Alive: timeout=";
        footer += std::to_string(keep_alive_timeout);
        footer += "\r\n\r\n";
    } else {
        footer = "Connection: close\r\n\r\n";
    }

    header.reserve(15 + code.size() + headers.size() + footer.size());
    header += "HTTP/1.1 ";
    header += code;
    header += " OK\r\n";
    header += headers;
    header += footer;
    return header;
}

Response request(const Url &url, http::Method method, const Data &data) {
    RequestOutgoming r;
    r.set_path(url.endpoint);
    r.set_method(method);
    r.set_content_type(data.content_type);
    r.set_data(std::string(data.data));
    return request(url, r);
}

std::pair<std::string, size_t> parse_response_header(Response& response, std::string_view header){
    std::size_t sp1 = header.find(' ');
    std::size_t sp2 = (sp1 == std::string::npos)
                          ? std::string::npos
                          : header.find(' ', sp1 + 1);
    unsigned code = 0;
    if (sp1 != std::string::npos && sp2 != std::string::npos) {
        for (std::size_t i = sp1 + 1; i < sp2; ++i) {
            char c = header[i];
            if (c >= '0' && c <= '9') {
                code = code * 10 + (c - '0');
            } else
                break;
        }
    }
    response.code = code;
    // Content-Length
    std::size_t content_length = 0;
    std::size_t line_start = header.find("\r\n");
    if (line_start != std::string::npos)
        line_start += 2;
    
    size_t header_size = header.size();
    size_t header_end = 0;
    while (line_start < header_size) {
        std::size_t line_end = header.find("\r\n", line_start);
        if (line_end == std::string::npos || line_end > header_size)
            break;
        std::size_t len = line_end - line_start;
        const char *ls = header.data() + line_start;
        static const char kcl[] = "content-length:";
        bool is_cl = content_length == 0 && len >= sizeof(kcl) - 1;
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
        header_end = line_start;
    }
    
    std::string leftover;
    if (header.size() > header_end) {
        leftover.assign(header.data() + header_end,
                        header.size() - header_end);
    }
    return {leftover, content_length};
}

template <class Socket>
std::string read_response_header(Socket& socket, std::array<char, 4096>& buf){
    std::string header;
    header.reserve(1024);
    asio::error_code ec;
    while (true) {
        std::size_t n = socket.read_some(
            asio::buffer(buf.data(), buf.size()), ec);
        if (ec && ec != asio::error::eof)
            break;
        header.append(buf.data(), n);
        auto pos = header.find("\r\n\r\n");
        if (pos != std::string::npos) {
            break;
        }
        if (ec == asio::error::eof)
            break;
    }
    return header;
}

template <class Socket>
void read_response_body(Response& response, size_t content_length, const std::string& leftover, Socket& socket, std::array<char, 4096>& buf){
    if (content_length > 0) {
        response.body.reserve(content_length);
        if (!leftover.empty()) {
            response.body.append(
                leftover.data(),
                std::min(leftover.size(), content_length));
        }
        while (response.body.size() < content_length) {
            asio::error_code ec2;
            std::size_t n = socket.read_some(asio::buffer(buf.data(), buf.size()), ec2);
            if (n > 0) {
                std::size_t need = content_length - response.body.size();
                response.body.append(buf.data(), std::min(need, n));
            }
            if (ec2)
                break;
        }
    } else {
        if (!leftover.empty())
            response.body.append(leftover);
        for (;;) {
            asio::error_code ec2;
            std::size_t n = socket.read_some(asio::buffer(buf), ec2);
            if (n > 0)
                response.body.append(buf.data(), n);
            if (ec2)
                break;
        }
    }
}

template <typename SocketLike>
static void apply_timeouts(SocketLike &sock, int connect_ms, int read_ms){
    auto fd = sock.native_handle();
    if (fd < 0)
        return;
    if (connect_ms > 0) {
        timeval tv{};
        tv.tv_sec = connect_ms / 1000;
        tv.tv_usec = (connect_ms % 1000) * 1000;
        ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    }
    if (read_ms > 0) {
        timeval tv{};
        tv.tv_sec = read_ms / 1000;
        tv.tv_usec = (read_ms % 1000) * 1000;
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }
}

Response send(Method method, const char* host, const char* port, const char* path, std::string&& body, const std::vector<std::pair<std::string, std::string>>& headers){
    
    http::Url url;
    url.host = host;
    url.port = std::atoi(port);
    url.endpoint = "/send_notification";

    http::RequestOutgoming request;
    for(auto&& [k, v] : headers){
        request.add_header(k, v);
    }
    request.set_data(std::move(body));
    request.set_method(http::Method::post);
    return http::request(url, request);
}
Response post(const char* host, const char* port, const char* path, std::string&& body, const std::vector<std::pair<std::string, std::string>>& headers){
    return send(Method::post, host, port, path, std::move(body), headers);
}
Response get(const char* host, const char* port, const char* path, std::string&& body, const std::vector<std::pair<std::string, std::string>>& headers){
    return send(Method::get, host, port, path, std::move(body), headers);
}

Response request(const Url &url, RequestOutgoming &request) {
    http::Response result{-1, ""};
    try {
        asio::io_service io_service;

        bool use_tls = url.https;
        std::string host = url.host;
        if (host.rfind("https://", 0) == 0) {
            use_tls = true;
            host.erase(0, 8);
        } else if (host.rfind("http://", 0) == 0) {
            use_tls = false;
            host.erase(0, 7);
        }

        asio::ip::tcp::resolver resolver(io_service);
        asio::ip::tcp::resolver::query query(host, std::to_string(url.port));
        auto endpoints = resolver.resolve(query);

        request.set_path(url.endpoint);
        std::string req  = request.get_http_body(host);

        if (!use_tls) {
            asio::ip::tcp::socket socket(io_service);
            asio::connect(socket, endpoints);
            apply_timeouts(socket, request.get_timeout_connect_ms(), request.get_timeout_read_ms());
            asio::write(socket, asio::buffer(req));

            std::array<char, 4096> buf;
            auto header = read_response_header(socket, buf);
            auto [leftover, content_length] = parse_response_header(result, header);
            read_response_body(result, content_length, leftover, socket, buf);
        } else {
            asio::ssl::context ctx(asio::ssl::context::tlsv12_client);
            asio::ssl::stream<asio::ip::tcp::socket> socket(io_service, ctx);
            socket.set_verify_mode(asio::ssl::verify_none);
            asio::connect(socket.lowest_layer(), endpoints);
            // apply_timeouts(socket, request.get_timeout_connect_ms(), request.get_timeout_read_ms());
            socket.handshake(asio::ssl::stream_base::client);
            asio::write(socket, asio::buffer(req));

            std::array<char, 4096> buf;
            auto header = read_response_header(socket, buf);
            auto [leftover, content_length] = parse_response_header(result, header);
            read_response_body(result, content_length, leftover, socket, buf);
        }
    } catch (std::exception &e) {
        log_error << "Error: " << e.what() << " URL: " << url.host << ":"
                  << url.port << url.endpoint << "<<"
                  << std::string(request.get_data()) << "\n";
    }
    return result;
}

} // namespace http

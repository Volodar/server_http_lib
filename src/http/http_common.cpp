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
const std::string CONTENT_LENGTH("Content-Length");
const std::string USER_AGENT("User-Agent");
const std::string ACCEPT_LANGUAGE("Accept-Language");

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

std::string Params::to_url_encoded() const {
    std::string result;
    result.reserve(_len * 3 + _params.size());
    size_t index = _params.size();
    for(const auto& [name, value] : _params) {
        result += url_encode(name);
        result += '=';
        result += url_encode(value);
        if(--index > 0) {
            result += '&';
        }
    }
    return result;
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
Response::Response(int code_, std::string&& body_)
: code(code_)
, body(std::move(body_)) {
    
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
const std::string Response::get_header(std::string_view name){
    for(auto header : _headers){
        if(header.first == name)
            return header.second;
    }
    return std::string();
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
struct ParsedHeader {
    std::string leftover;
    std::size_t content_length = 0;
    bool chunked = false;
};

static inline bool ieq_prefix(const char* s, std::size_t n, const char* lit) {
    // case-insensitive prefix compare for ASCII
    for (std::size_t i = 0; lit[i] && i < n; ++i) {
        char a = s[i];
        char b = lit[i];
        if (a >= 'A' && a <= 'Z') a += 32;
        if (b >= 'A' && b <= 'Z') b += 32;
        if (a != b) return false;
    }
    return true;
}

template <class Socket>
static bool read_more(Socket& socket, std::string& stash, std::array<char, 4096>& buf) {
    asio::error_code ec;
    std::size_t n = socket.read_some(asio::buffer(buf.data(), buf.size()), ec);
    if (n > 0) stash.append(buf.data(), n);
    if (ec) return false; // eof or error
    return true;
}

static bool try_get_line(std::string& stash, std::string& line) {
    std::size_t pos = stash.find("\r\n");
    if (pos == std::string::npos) return false;
    line.assign(stash.data(), pos);
    stash.erase(0, pos + 2);
    return true;
}

static bool parse_hex_size_line(const std::string& line, std::size_t& out_size) {
    // chunk-size [; extensions]
    std::size_t i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;

    std::size_t val = 0;
    bool any = false;
    for (; i < line.size(); ++i) {
        char c = line[i];
        if (c == ';') break;
        int d = -1;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = 10 + (c - 'a');
        else if (c >= 'A' && c <= 'F') d = 10 + (c - 'A');
        else if (c == ' ' || c == '\t') continue;
        else return false;
        any = true;
        val = (val << 4) + std::size_t(d);
    }
    if (!any) return false;
    out_size = val;
    return true;
}

template <class Socket>
static void read_response_body_chunked(Response& response,
                                      std::string stash,
                                      Socket& socket,
                                      std::array<char, 4096>& buf) {
    response.body.clear();

    for (;;) {
        // 1) read chunk-size line
        std::string line;
        while (!try_get_line(stash, line)) {
            if (!read_more(socket, stash, buf)) return; // connection closed/error
        }

        std::size_t chunk_size = 0;
        if (!parse_hex_size_line(line, chunk_size)) {
            return; // malformed
        }

        if (chunk_size == 0) {
            // 2) read trailers until CRLF CRLF (optional). We can just consume them and stop.
            // Need an empty line that ends trailers: a line == ""
            for (;;) {
                while (!try_get_line(stash, line)) {
                    if (!read_more(socket, stash, buf)) break;
                }
                if (line.empty()) break;
            }
            return;
        }

        // 3) ensure we have chunk_size + CRLF
        while (stash.size() < chunk_size + 2) {
            if (!read_more(socket, stash, buf)) return;
        }

        response.body.append(stash.data(), chunk_size);
        stash.erase(0, chunk_size);

        // 4) consume CRLF after data
        if (stash.size() < 2) {
            while (stash.size() < 2) {
                if (!read_more(socket, stash, buf)) return;
            }
        }
        if (!(stash[0] == '\r' && stash[1] == '\n')) {
            return; // malformed
        }
        stash.erase(0, 2);
    }
}

ParsedHeader parse_response_header(Response& response, const std::string& raw) {
    ParsedHeader out{};

    // split headers / leftover by \r\n\r\n
    std::size_t header_end = raw.find("\r\n\r\n");
    std::size_t headers_len = (header_end == std::string::npos) ? raw.size() : (header_end + 4);

    if (raw.size() > headers_len) {
        out.leftover.assign(raw.data() + headers_len, raw.size() - headers_len);
    }

    // status line
    std::size_t line0_end = raw.find("\r\n");
    std::string_view status = (line0_end == std::string::npos)
        ? std::string_view(raw)
        : std::string_view(raw.data(), line0_end);

    std::size_t sp1 = status.find(' ');
    std::size_t sp2 = (sp1 == std::string_view::npos) ? std::string_view::npos : status.find(' ', sp1 + 1);
    unsigned code = 0;
    if (sp1 != std::string_view::npos && sp2 != std::string_view::npos) {
        for (std::size_t i = sp1 + 1; i < sp2; ++i) {
            char c = status[i];
            if (c >= '0' && c <= '9') code = code * 10 + (c - '0');
            else break;
        }
    }
    response.code = code;

    // iterate header lines
    std::size_t line_start = (line0_end == std::string::npos) ? raw.size() : (line0_end + 2);
    while (line_start < headers_len) {
        std::size_t line_end = raw.find("\r\n", line_start);
        if (line_end == std::string::npos || line_end > headers_len) break;
        std::size_t len = line_end - line_start;
        const char* ls = raw.data() + line_start;
        if (len == 0) {
            break;
        }

        std::size_t colon = line_start;
        while (colon < line_end && raw[colon] != ':') {
            ++colon;
        }
        if (colon < line_end) {
            std::string name = raw.substr(line_start, colon - line_start);
            std::size_t value_start = colon + 1;
            while (value_start < line_end && (raw[value_start] == ' ' || raw[value_start] == '\t')) {
                ++value_start;
            }
            response.add_header(name, raw.substr(value_start, line_end - value_start));
        }

        // Content-Length:
        if (out.content_length == 0 && len >= 15 && ieq_prefix(ls, len, "content-length:")) {
            std::size_t i = 15;
            while (i < len && (ls[i] == ' ' || ls[i] == '\t')) ++i;
            std::size_t val = 0;
            while (i < len && ls[i] >= '0' && ls[i] <= '9') {
                val = val * 10 + (ls[i] - '0');
                ++i;
            }
            out.content_length = val;
        }

        // Transfer-Encoding:
        if (len >= 18 && ieq_prefix(ls, len, "transfer-encoding:")) {
            // very simple contains("chunked") (case-insensitive)
            for (std::size_t i = 18; i + 6 < len; ++i) {
                char c0 = ls[i+0], c1 = ls[i+1], c2 = ls[i+2], c3 = ls[i+3], c4 = ls[i+4], c5 = ls[i+5], c6 = ls[i+6];
                auto low = [](char x){ return (x >= 'A' && x <= 'Z') ? char(x + 32) : x; };
                if (low(c0)=='c' && low(c1)=='h' && low(c2)=='u' && low(c3)=='n' && low(c4)=='k' && low(c5)=='e' && low(c6)=='d') {
                    out.chunked = true;
                    break;
                }
            }
        }

        line_start = line_end + 2;
    }

    return out;
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
void read_response_body(Response& response,
                        std::size_t content_length,
                        bool chunked,
                        const std::string& leftover,
                        Socket& socket,
                        std::array<char, 4096>& buf) {

    if (chunked) {
        read_response_body_chunked(response, leftover, socket, buf);
        return;
    }

    if (content_length > 0) {
        response.body.clear();
        response.body.reserve(content_length);

        if (!leftover.empty()) {
            response.body.append(leftover.data(),
                                 std::min(leftover.size(), content_length));
        }

        while (response.body.size() < content_length) {
            asio::error_code ec2;
            std::size_t n = socket.read_some(asio::buffer(buf.data(), buf.size()), ec2);
            if (n > 0) {
                std::size_t need = content_length - response.body.size();
                response.body.append(buf.data(), std::min(need, n));
            }
            if (ec2) break;
        }
        return;
    }

    // no CL, not chunked: read to EOF
    response.body.clear();
    if (!leftover.empty()) response.body.append(leftover);
    for (;;) {
        asio::error_code ec2;
        std::size_t n = socket.read_some(asio::buffer(buf), ec2);
        if (n > 0) response.body.append(buf.data(), n);
        if (ec2) break;
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
    url.endpoint = path;

    http::RequestOutgoming request;
    for(auto&& [k, v] : headers){
        request.add_header(k, v);
    }
    request.set_data(std::move(body));
    request.set_method(method);
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
            auto header_raw = read_response_header(socket, buf);
            auto ph = parse_response_header(result, header_raw);
            read_response_body(result, ph.content_length, ph.chunked, ph.leftover, socket, buf);
        } else {
            asio::ssl::context ctx(asio::ssl::context::tlsv12_client);
            asio::ssl::stream<asio::ip::tcp::socket> socket(io_service, ctx);
            socket.set_verify_mode(asio::ssl::verify_none);
            asio::connect(socket.lowest_layer(), endpoints);
            // apply_timeouts(socket, request.get_timeout_connect_ms(), request.get_timeout_read_ms());
            socket.handshake(asio::ssl::stream_base::client);
            asio::write(socket, asio::buffer(req));

            std::array<char, 4096> buf;
            auto header_raw = read_response_header(socket, buf);
            auto ph = parse_response_header(result, header_raw);
            read_response_body(result, ph.content_length, ph.chunked, ph.leftover, socket, buf);
        }
    } catch (std::exception &e) {
        log_error << "Error: " << e.what() << " URL: " << url.host << ":"
                  << url.port << url.endpoint << "<<"
                  << std::string(request.get_data()) << "\n";
    }
    return result;
}

} // namespace http

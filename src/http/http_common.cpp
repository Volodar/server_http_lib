#include "http_common.h"
#include "asio/ssl.hpp"
#include "utils.h"
#include <algorithm>
#include <array>
#include <asio.hpp>
#include <cctype>
#include <cstring>
#include <list>
#include <charconv>
#include "Log.h"

namespace http {

const std::string string_empty;
const std::string CONTENT_TYPE("Content-Type");
const std::string USER_AGENT("User-Agent");

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
    // last write wins
    _params.insert_or_assign(name, value);
}
// std::string Params::operator[](const std::string& name)
//{
//     return _params[name];
// }
bool Params::empty() const { return _params.empty(); }
size_t Params::size() const { return _params.size(); }
std::string Params::to_string(char delimiter) const {
    std::string res;
    size_t index = _params.size();
    for (auto &pair : _params) {
        res += std::string(pair.first) + "=" + std::string(pair.second);
        if (--index > 0)
            res += delimiter;
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
    if(is_equal_lower(s, size, "del")) return Method::del;
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

Request::Request(std::string&& h)
: _header(std::move(h)) {
    parse_header();
}

void Request::parse_header() {
    std::string_view hdr = _header;

    auto nl = hdr.find('\n');
    if (nl == std::string_view::npos)
        return;
    auto line = hdr.substr(0, nl);
    if (!line.empty() && line.back() == '\r')
        line.remove_suffix(1);

    // tokens by first and second space
    auto p1 = line.find(' ');
    auto p2 = (p1 == std::string_view::npos) ? std::string_view::npos
                                             : line.find(' ', p1 + 1);
    std::string_view method_sv =
        (p1 == std::string_view::npos) ? line : line.substr(0, p1);
    std::string_view target_sv =
        (p1 == std::string_view::npos || p2 == std::string_view::npos)
            ? std::string_view{}
            : line.substr(p1 + 1, p2 - (p1 + 1));

    this->_method = strToMethod(std::string(method_sv));

    // path and query params
    if (!target_sv.empty()) {
        auto qpos = target_sv.find('?');
        if (qpos == std::string_view::npos) {
            this->_path = target_sv;
        } else {
            this->_path = target_sv.substr(0, qpos);
            std::string_view query = target_sv.substr(qpos + 1);
            while (!query.empty()) {
                auto amp = query.find('&');
                std::string_view token = (amp == std::string_view::npos)
                                             ? query
                                             : query.substr(0, amp);
                if (amp == std::string_view::npos)
                    query = std::string_view{};
                else
                    query.remove_prefix(amp + 1);
                if (token.empty())
                    continue;
                auto eq = token.find('=');
                if (eq == std::string_view::npos) {
                    _params.set(token, std::string_view{});
                } else {
                    auto k = token.substr(0, eq);
                    auto v = token.substr(eq + 1);
                    _params.set(k, v);
                }
            }
        }
    }
    _headers_pos = nl;
}

void Request::parse_headers() const {
    std::string_view hdr = _header;
    size_t nl = _headers_pos;
    if(nl >= hdr.size())
        return;
    // 2) Headers until blank line
    std::string_view rest = hdr.substr(nl + 1);
    while (true) {
        auto nl2 = rest.find('\n');
        if (nl2 == std::string_view::npos)
            break;
        auto hline = rest.substr(0, nl2);
        rest.remove_prefix(nl2 + 1);
        if (!hline.empty() && hline.back() == '\r')
            hline.remove_suffix(1);
        if (hline.empty())
            break; // end of headers

        auto colon = hline.find(':');
        if (colon == std::string_view::npos)
            continue;
        std::string_view name = hline.substr(0, colon);
        std::string_view value = hline.substr(colon + 1);
        sv_strip(name);
        sv_strip(value);
        _headers.set(name, value);
    }
}

void Request::parse_post_data_params() const {
    std::string_view data = _post_data;
    while (!data.empty()) {
        auto amp = data.find('&');
        std::string_view token =
            (amp == std::string_view::npos) ? data : data.substr(0, amp);
        if (amp == std::string_view::npos)
            data = std::string_view{};
        else
            data.remove_prefix(amp + 1);

        if (token.empty())
            continue;

        auto eq = token.find('=');
        if (eq == std::string_view::npos) {
            _post_data_params.set(token, std::string_view{});
        } else {
            auto k = token.substr(0, eq);
            auto v = token.substr(eq + 1);
            _post_data_params.set(k, v);
        }
    }
}

void Request::parse_cookie_params() const {
    std::string_view cookies = get_headers().get("Cookie");
    while (!cookies.empty()) {
        auto sc = cookies.find(';');
        std::string_view token =
            (sc == std::string_view::npos) ? cookies : cookies.substr(0, sc);
        if (sc == std::string_view::npos)
            cookies = std::string_view{};
        else
            cookies.remove_prefix(sc + 1);
        if (token.empty())
            continue;

        // trim token
        sv_strip(token);
        auto eq = token.find('=');
        if (eq == std::string_view::npos)
            continue;

        std::string_view k = token.substr(0, eq);
        std::string_view v = token.substr(eq + 1);
        sv_strip(k);
        sv_strip(v);
        _cookie_params.set(k, v);
    }
}

std::string_view Request::get_post_data_param(const std::string& name) const {
    return get_post_data_params().get(name);
}

std::string_view Request::get_cookie_value(const std::string& name) const {
    return get_cookie_params().get(name);
}

void Request::set_user_ip(const std::string& value) { _user_ip = value; }
void Request::set_path(std::string_view value) { _path = value; }
void Request::set_content_type(std::string_view value) {
    _headers.set(CONTENT_TYPE, value);
}
void Request::set_data(std::string&& value) {
    _post_data = std::move(value);
}
void Request::set_method(Method method) { this->_method = method; }
Method Request::get_method() const { return _method; }
std::string_view Request::get_path() const { return _path; }
std::string_view Request::get_user_agent() const {
    return get_headers().get(USER_AGENT);
}
std::string_view Request::get_content_type() const {
    return get_headers().get(CONTENT_TYPE);
}
std::string_view Request::get_data() const { return _post_data; }
const std::string& Request::get_user_ip() const { return _user_ip; }
const Params &Request::get_params() const { return _params; }
const Params &Request::get_post_data_params() const {
    if (_post_data_params.empty())
        parse_post_data_params();
    return _post_data_params;
}
const Params &Request::get_headers() const {
    if (_headers.empty())
        parse_headers();
    return _headers;
}
const Params &Request::get_cookie_params() const {
    if (_cookie_params.empty())
        parse_cookie_params();
    return _cookie_params;
}

std::vector<std::string_view> Request::get_accept_language() const{
    std::list<std::pair<float, std::string_view>> list;
    auto lang = get_headers().get("Accept-Language");
    auto variants = sv_split(lang, ',');
    for(auto variant : variants){
        auto parts = sv_split(variant, ';');
        if(parts.size() == 2){
            //TODO: change to std::from_chars
            //Sorry. on xcode from_chars has onlyt to integral types :(
            auto priority = parts.at(1);
            while(!std::isdigit(priority[0]))
                priority.remove_prefix(1);
            float value = std::stof(std::string(priority));
            list.push_back({value, parts.at(0)});
        } else {
            list.push_back({1, parts.at(0)});
        }
    }
    list.sort([](auto& lhs, auto& rhs){
        return lhs.first > rhs.first;
    });
    
    std::vector<std::string_view> result;
    result.reserve(list.size());
    for(auto iter = list.begin(); iter != list.end(); ++iter){
        result.push_back(iter->second);
    }
    return result;
}

Response::Response()
: code(-1) {
}
Response::Response(int code, const std::string& body) {
    this->code = code;
    this->body = body;
}
void Response::add_header(const std::string& header) {
    headers.push_back(header);
}
void Response::add_header(const std::string& name, const std::string& value) {
    headers.push_back(name + ": " + value);
}
void Response::add_header_content_type(const std::string& type) {
    add_header(CONTENT_TYPE, type);
}

Response request(const Url &url, http::Method method, const Data &data) {
    std::string header_empty;
    Request r(std::move(header_empty));
    r.set_method(method);
    r.set_content_type(data.content_type);
    r.set_data(std::string(data.data));
    return request(url, r);
}

Response request(const Url &url, const Request &request) {
    http::Response result{-1, ""};
    try {
        asio::io_service io_service;

        // Normalize host and determine TLS
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

        // Build request using std::string (no stringstreams)
        std::string req;
        const bool has_body = !request.get_data().empty();
        // Rough reserve to minimize reallocations
        req.reserve(64 + url.endpoint.size() + host.size() +
                    (has_body ? (64 + request.get_data().size()) : 0));
        req += methodToStr(request.get_method());
        req += ' ';
        req += url.endpoint;
        req += " HTTP/1.1\r\n";
        req += "Host: ";
        req += host;
        req += "\r\n";
        if (has_body) {
            req += "Content-Type: ";
            req += std::string(request.get_content_type());
            req += "\r\n";
            req += "Content-Length: ";
            req += std::to_string(request.get_data().size());
            req += "\r\n";
        }
        req += "Connection: close\r\n\r\n";
        if (has_body) {
            auto body_view = request.get_data();
            req.append(body_view.data(), body_view.size());
        }

        if (!use_tls) {
            asio::ip::tcp::socket socket(io_service);
            asio::connect(socket, endpoints);
            asio::write(socket, asio::buffer(req));
            // Manual read + parse (no streambuf/istream)
            {
                std::string header;
                header.reserve(1024);
                std::array<char, 4096> buf;
                std::size_t header_end = std::string::npos;
                asio::error_code ec;
                std::string leftover;
                while (true) {
                    std::size_t n = socket.read_some(
                        asio::buffer(buf.data(), buf.size()), ec);
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
                // Parse status code: HTTP/x.y <code>
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
                            std::size_t need =
                                content_length - result.body.size();
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
                        std::size_t n =
                            socket.read_some(asio::buffer(buf), ec2);
                        if (n > 0)
                            result.body.append(buf.data(), n);
                        if (ec2)
                            break;
                    }
                }
            }
        } else {
            asio::ssl::context ctx(asio::ssl::context::tlsv12_client);
            asio::ssl::stream<asio::ip::tcp::socket> socket(io_service, ctx);
            // Note: For self-signed/local servers we disable verification.
            socket.set_verify_mode(asio::ssl::verify_none);
            asio::connect(socket.lowest_layer(), endpoints);
            socket.handshake(asio::ssl::stream_base::client);
            asio::write(socket, asio::buffer(req));
            // Manual read + parse (no streambuf/istream) for SSL
            {
                std::string header;
                header.reserve(1024);
                std::array<char, 4096> buf;
                std::size_t header_end = std::string::npos;
                asio::error_code ec;
                std::string leftover;
                while (true) {
                    std::size_t n = socket.read_some(
                        asio::buffer(buf.data(), buf.size()), ec);
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
                        if (c >= '0' && c <= '9') {
                            code = code * 10 + (c - '0');
                        } else
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
                            std::size_t need =
                                content_length - result.body.size();
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
                        std::size_t n =
                            socket.read_some(asio::buffer(buf), ec2);
                        if (n > 0)
                            result.body.append(buf.data(), n);
                        if (ec2)
                            break;
                    }
                }
            }
        }
    } catch (std::exception &e) {
        log_error << "Error: " << e.what() << " URL: " << url.host << ":"
                  << url.port << url.endpoint << "<<"
                  << std::string(request.get_data()) << "\n";
    }
    return result;
}

} // namespace http

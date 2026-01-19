//
//  InRequest.cpp
//  http
//
//  Created by Vladimir Tolmachev on 18.01.2026.
//

#include "RequestIncoming.h"
#include "utils.h"

namespace http {

RequestIncoming::RequestIncoming(std::string&& header)
: _header(std::move(header)) {
    parse_header();
}

void RequestIncoming::parse_header() {
    std::string_view hdr = _header;
    
    auto nl = hdr.find('\n');
    if (nl == std::string_view::npos)
        return;
    auto line = hdr.substr(0, nl);
    if (!line.empty() && line.back() == '\r')
        line.remove_suffix(1);
    
    auto p1 = line.find(' ');
    auto p2 = (p1 == std::string_view::npos) ? std::string_view::npos : line.find(' ', p1 + 1);
    std::string_view method_sv = (p1 == std::string_view::npos) ? line : line.substr(0, p1);
    std::string_view target_sv = (p1 == std::string_view::npos || p2 == std::string_view::npos)
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
                std::string_view token = (amp == std::string_view::npos) ? query : query.substr(0, amp);
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

void RequestIncoming::parse_headers() const {
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

void RequestIncoming::parse_post_data_params() const {
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

void RequestIncoming::parse_cookie_params() const {
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

}

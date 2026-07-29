//
//  RequestOutgoming.cpp
//  http
//
//  Created by Vladimir Tolmachev on 18.01.2026.
//

#include "RequestOutgoming.h"
#include "http_common.h"
#include <cassert>

namespace http{

RequestOutgoming::RequestOutgoming()
: Request() {
}

void RequestOutgoming::set_method(Method method) {
    this->_method = method;
}
void RequestOutgoming::set_path(const std::string& value) {
    _path = value;
}
void RequestOutgoming::set_content_type(const std::string& value) {
    auto iter = _headers.insert({CONTENT_TYPE, value});
    Request::_headers.set(CONTENT_TYPE, iter.first->second);
}
void RequestOutgoming::set_params(const std::string& name, const std::string& value){
    auto iter = _params.insert({name, value});
    Request::_params.set(iter.first->first, iter.first->second);
}
void RequestOutgoming::set_post_params(const std::string& name, const std::string& value){
    auto iter = _post_params.insert_or_assign(name, value).first;
    Request::_post_data_params.set(iter->first, iter->second);
}
void RequestOutgoming::add_header(const std::string& name, const std::string& value){
    auto iter = _headers.insert({name, value});
    Request::_headers.set(iter.first->first, iter.first->second);
}

void RequestOutgoming::parse_header(){
}
void RequestOutgoming::parse_headers() const{
}
void RequestOutgoming::parse_post_data_params() const{
}
void RequestOutgoming::parse_cookie_params() const{
}

std::string RequestOutgoming::get_http_body(const std::string& host) const{
    const bool has_post_params = !_post_params.empty();
    const std::string post_data = has_post_params ? get_post_data_params().to_url_encoded() : _post_data;
    const bool has_body = !post_data.empty();
    if (has_body) {
        auto non_const = const_cast<RequestOutgoming*>(this);
        if(!Request::_headers.has(CONTENT_TYPE)) {
            non_const->add_header(CONTENT_TYPE, has_post_params ? "application/x-www-form-urlencoded" : std::string(get_content_type()));
        }
        if(!Request::_headers.has(CONTENT_LENGTH))
            non_const->add_header(CONTENT_LENGTH, std::to_string(post_data.size()));
    }
    
    std::string headers;
    headers.reserve(get_headers().get_buffer_lenght() + 4 * get_headers().size());
    for (auto &h : get_headers()) {
        headers += h.first;
        headers += ": ";
        headers += h.second;
        headers += "\r\n";
    }
    auto params = get_params().to_url_encoded();
    std::string result;
    
    static const std::string empty;
    std::string content_type(has_body ? get_content_type() : empty);
    std::string content_lenght(has_body ? std::to_string(_post_data.size()) : empty);
    std::string method = methodToStr(get_method());
    
    auto buffer_size = 47 + //http version and other
                        1 + method.size() + _path.size() + //METHOD + path
                        1 + params.size() + //get params
                        headers.size() + // headers
                        host.size() + // host
                        post_data.size();
    result.reserve(buffer_size);
    
    result += std::move(method);
    result += " ";
    result += _path;
    if(!get_params().empty()){
        result += "?";
        result += std::move(params);
    }
    result += " HTTP/1.1\r\nHost: ";
    result += host;
    result += "\r\n";
    result += std::move(headers);
    result += "Connection: close\r\n\r\n";
    if (has_body)
        result += post_data;
    
    return result;
}

}

//
//  Request.cpp
//  http
//
//  Created by Vladimir Tolmachev on 18.01.2026.
//

#include "Request.h"
#include "utils.h"
#include <list>

namespace http {
const std::string CONTENT_TYPE("Content-Type");
const std::string USER_AGENT("User-Agent");


Request::Request(){
}

std::string_view Request::get_post_data_param(std::string_view name) const {
    return get_post_data_params().get(name);
}

std::string_view Request::get_cookie_value(const std::string& name) const {
    return get_cookie_params().get(name);
}

void Request::set_data(std::string&& value) {
    _post_data = std::move(value);
}
Method Request::get_method() const { return _method; }
std::string_view Request::get_path() const { return _path; }
std::string_view Request::get_user_agent() const {
    return get_headers().get(USER_AGENT);
}
std::string_view Request::get_content_type() const {
    return get_headers().get(CONTENT_TYPE);
}
std::string_view Request::get_data() const { return _post_data; }
std::string&& Request::move_data() {
    return std::move(_post_data);
}
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

}

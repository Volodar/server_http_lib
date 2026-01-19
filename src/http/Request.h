//
//  Request.hpp
//  http
//
//  Created by Vladimir Tolmachev on 18.01.2026.
//

#ifndef Request_hpp
#define Request_hpp

#include <string>
#include "http_common.h"

namespace http {

class Request {
public:
    Request(const std::string& header) = delete;
    Request(const char* header) = delete;
    Request(const Request&) = delete;
    const Request& operator=(const Request&) = delete;
protected:
    Request();
    virtual ~Request() = default;
public:
    virtual void set_data(std::string&& value);

    Method get_method() const;
    std::string_view get_path() const;
    std::string_view get_data() const;
    std::string&& move_data();
    const Params &get_params() const;
    const Params &get_post_data_params() const;
    const Params &get_headers() const;
    const Params &get_cookie_params() const;

    std::string_view get_user_agent() const;
    std::string_view get_content_type() const;
    std::string_view get_post_data_param(const std::string& name) const;
    std::string_view get_cookie_value(const std::string& name) const;
    
    std::vector<std::string_view> get_accept_language() const;

protected:
    virtual void parse_header() = 0;
    virtual void parse_headers() const = 0;
    virtual void parse_post_data_params() const = 0;
    virtual void parse_cookie_params() const = 0;

protected:
    Method _method;
    Params _params;
    mutable Params _headers;
    mutable Params _post_data_params;
    mutable Params _cookie_params;
    std::string _post_data;
    std::string _path;
};

}

#endif /* Request_hpp */

//
//  RequestOutgoming.hpp
//  http
//
//  Created by Vladimir Tolmachev on 18.01.2026.
//

#ifndef RequestOutgoming_hpp
#define RequestOutgoming_hpp

#include "Request.h"
#include <unordered_map>

namespace http{

class RequestOutgoming : public Request{
public:
    explicit RequestOutgoming();
    RequestOutgoming(const RequestOutgoming&) = delete;
    RequestOutgoming(RequestOutgoming&&) noexcept = delete;
    RequestOutgoming& operator = (const RequestOutgoming&) = delete;
    RequestOutgoming& operator = (RequestOutgoming&&) noexcept = delete;
public:
    void set_path(const std::string& value);
    void set_method(Method method);
    void set_content_type(const std::string& value);
    void set_params(const std::string& name, const std::string& value);
    void add_header(const std::string& name, const std::string& value);
    void set_timeout_ms(int connect, int read);
    
    std::string get_http_body(const std::string& host) const;
    int get_timeout_connect_ms() const { return _connect_timeout_ms; }
    int get_timeout_read_ms() const { return _read_timeout_ms; }
protected:
    virtual void parse_header();
    virtual void parse_headers() const;
    virtual void parse_post_data_params() const;
    virtual void parse_cookie_params() const;
private:
    std::unordered_map<std::string, std::string> _params;
    std::unordered_map<std::string, std::string> _headers;
    std::unordered_map<std::string, std::string> _post_data_params;
    int _connect_timeout_ms = 0;
    int _read_timeout_ms = 0;
};

}

#endif /* RequestOutgoming_hpp */

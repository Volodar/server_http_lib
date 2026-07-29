//
//  InRequest.hpp
//  http
//
//  Created by Vladimir Tolmachev on 18.01.2026.
//

#ifndef InRequest_hpp
#define InRequest_hpp

#include "Request.h"
#include <unordered_map>

namespace http{

class RequestIncoming : public Request{
public:
    explicit RequestIncoming(std::string&& header);

    void set_user_ip(const std::string& value) { _user_ip = value; }
    const std::string& get_user_ip() const { return _user_ip; }
    
    std::string_view get(std::string_view name, bool require) const;
    std::string_view get_post(std::string_view name, bool require) const;
protected:
    virtual void parse_header() override;
    virtual void parse_headers() const override;
    virtual void parse_post_data_params() const override;
    virtual void parse_cookie_params() const override;
private:
    std::string _user_ip;
    std::string _header;
    std::unordered_map<std::string, std::string> _decoded_params;
    mutable std::unordered_map<std::string, std::string> _decoded_post_data_params;
    size_t _headers_pos = -1;
    mutable bool _post_data_params_parsed = false;
};

}
#endif /* InRequest_hpp */

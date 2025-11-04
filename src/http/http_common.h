#ifndef http_common
#define http_common

#include <string>
#include <unordered_map>
#include <vector>

namespace http {

class Url {
  public:
    std::string host;
    int port;
    std::string endpoint;
    bool https = false; // use TLS when true
};

enum class Method { get, post, put, del, create };

class Params {
  public:
    bool has(std::string_view name) const;

    template <class T> T get(std::string_view name) const;

    std::string_view get(std::string_view name) const;
    void set(std::string_view name, std::string_view value);
    bool empty() const;
    size_t size() const;
    std::string to_string(char delimiter='&') const;

    std::unordered_map<std::string_view, std::string_view>::iterator
    begin() noexcept {
        return _params.begin();
    }
    std::unordered_map<std::string_view, std::string_view>::iterator
    end() noexcept {
        return _params.end();
    }
    std::unordered_map<std::string_view, std::string_view>::const_iterator
    begin() const noexcept {
        return _params.begin();
    }
    std::unordered_map<std::string_view, std::string_view>::const_iterator
    end() const noexcept {
        return _params.end();
    }

  private:
    std::unordered_map<std::string_view, std::string_view> _params;
};

class Data {
  public:
    const std::string_view content_type;
    const std::string_view data;

    operator bool() const { return !data.empty(); }
};

class Response;

class Request {
  public:
    Request(const std::string& header);

    void set_user_ip(const std::string& value);
    void set_path(std::string_view value);
    void set_content_type(std::string_view value);
    void set_data(std::string_view value);
    void set_method(Method method);

    Method get_method() const;
    std::string_view get_path() const;
    std::string_view get_data() const;
    const std::string& get_user_ip() const;
    const Params &get_params() const;
    const Params &get_post_data_params() const;
    const Params &get_headers() const;
    const Params &get_cookie_params() const;

    std::string_view get_user_agent() const;
    std::string_view get_content_type() const;
    std::string_view get_post_data_param(const std::string& name) const;
    std::string_view get_cookie_value(const std::string& name) const;
    
    std::vector<std::string_view> get_accept_language() const;

  private:
    void parse_header();
    void parse_headers() const;
    void parse_post_data_params() const;
    void parse_cookie_params() const;

  private:
    const std::string& _header;

    Method _method;
    std::string_view _path;
    Params _params;
    mutable Params _headers;
    mutable Params _post_data_params;
    mutable Params _cookie_params;
    std::string_view _post_data;
    std::string _user_ip;
    size_t _headers_pos = -1;
};

class Response {
  public:
    Response();
    Response(int code, const std::string& body = "");
    void add_header(const std::string& header);
    void add_header(const std::string& name, const std::string& value);
    void add_header_content_type(const std::string& type);

  public:
    int code;
    std::string body;
    std::vector<std::string> headers;
};

extern Response ResponseNone;
extern Response ResponseOk;
extern Response Response403;
extern Response Response404;

// Унифицированный HTTP‑запрос. Если data непустой — тело включается и для GET.
Response request(const Url &url, Method method, const Data &data = {});
Response request(const Url &url, const Request &request);

Method strToMethod(const std::string& methodStr);
std::string methodToStr(Method method);

} // namespace http

#endif

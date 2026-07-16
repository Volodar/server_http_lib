#ifndef http_common
#define http_common

#include <string>
#include <unordered_map>
#include <vector>

namespace http {

class Request;
class RequestOutgoming;

class Url {
  public:
    std::string host;
    int port = 0;
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
    std::unordered_map<std::string_view, std::string_view>::const_iterator
    find(std::string_view name) const {
        return _params.find(name);
    }
    size_t get_buffer_lenght() const { return _len; }

private:
    size_t _len = 0;
    std::unordered_map<std::string_view, std::string_view> _params;
};

class Data {
  public:
    const std::string content_type;
    const std::string data;

    operator bool() const { return !data.empty(); }
};

class Response;

class Response {
  public:
    Response();
    Response(int code, const std::string& body = "");
    Response(int code, std::string&& body);
    void add_header(const std::string& name, const std::string& value);
    void add_header_content_type(const std::string& type);

    int get_code() const { return code; }
    std::string& get_body() { return body; }
    const std::vector<std::pair<std::string, std::string>>& get_headers() const { return _headers; }
    const std::string get_header(std::string_view name);
    
    std::string get_http_header(bool keep_alive, size_t _keep_alive_timeout) const;
public:
    int code;
    std::string body;
private:
    std::vector<std::pair<std::string, std::string>> _headers;
    size_t _headers_buffer_len = 0;
};

extern Response ResponseNone;
extern Response ResponseOk;
extern Response Response403;
extern Response Response404;

Response post(const char* host, const char* port, const char* path, std::string&& body, const std::vector<std::pair<std::string, std::string>>& headers);
Response get(const char* host, const char* port, const char* path, std::string&& body, const std::vector<std::pair<std::string, std::string>>& headers);
Response request(const Url &url, Method method, const Data &data = {});
Response request(const Url &url, RequestOutgoming &request);

Method strToMethod(const std::string& methodStr);
std::string methodToStr(Method method);
std::ostream& operator<<(std::ostream& os, Method method);


extern const std::string CONTENT_TYPE;
extern const std::string CONTENT_LENGTH;
extern const std::string USER_AGENT;
extern const std::string ACCEPT_LANGUAGE;

} // namespace http

#endif

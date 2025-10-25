//
//  http::RequestHandlers.hpp
//  dungeon_mobile_site
//
//  Created by Vladimir Tolmachev on 15.04.2025.
//

#ifndef RequestHandlers_hpp
#define RequestHandlers_hpp

#include "http/Server.h"
#include "http/http_common.h"

namespace http {

class ServerApp;

class RequestHandler {
  public:
    RequestHandler(ServerApp &app) : _app(app) {}
    virtual ~RequestHandler() = default;
    virtual http::Response handle(const http::Request &request) = 0;
    void set_sequire(const Handler &handler) { _sequire_handler = handler; }
    const Handler &get_sequire_handler() { return _sequire_handler; }

  protected:
    ServerApp &_app;
    Handler _sequire_handler;
};

class FileContent : public RequestHandler {
  public:
    FileContent(ServerApp &app);
    virtual http::Response handle(const http::Request &request) override;
};

class Redirect : public RequestHandler {
  public:
    Redirect(ServerApp &app, const std::string &redirect);
    virtual http::Response handle(const http::Request &request) override;

  private:
    std::string _redirect;
};

class Handler404 : public RequestHandler {
  public:
    Handler404(ServerApp &app);
    virtual http::Response handle(const http::Request &request) override;
};

// Common health endpoint handler: returns {"status":"ok"}
class HealthHandler : public RequestHandler {
  public:
    HealthHandler(ServerApp &app);
    virtual http::Response handle(const http::Request &request) override;
};

} // namespace http

#endif /* http::RequestHandlers_hpp */

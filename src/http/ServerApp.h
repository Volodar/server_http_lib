//
//  ServerApp.hpp
//  dungeon2_server_analytics
//
//  Created by Vladimir Tolmachev on 14.04.2025.
//

#ifndef ServerApp_hpp
#define ServerApp_hpp

#include "http/Handlers.h"
#include "http/Server.h"
#include <list>
#include <memory>
#include "LRUCache.h"

#if SERVER_WEB_HAVE_MYSQLCONNECTOR == 1
#include "http/mysql_wrapper.h"
#endif
#include "http/Scheduler.h"

namespace http {

const int require = 1;
const int optional = 0;

class CheckApiParam{
public:
    std::string name;
    std::string type;
    int is_require = require;
};

using GetParams = std::vector<CheckApiParam>;
using PostParams = std::vector<CheckApiParam>;
using Headers = std::vector<CheckApiParam>;

class ServerApp {
  public:
    ServerApp();
    ServerApp(int http_port);
    ServerApp(int http_port, int https_port, asio::ssl::context *ssl_ctx);
    void connect_mysql();
    void run(int num_threads = -1);
    
    void set_lru_cache(size_t capacity_mb);
    std::shared_ptr<LRUCache> get_lru_cache();

    asio::io_context &get_context() { return _context; };

#if SERVER_WEB_HAVE_MYSQLCONNECTOR == 1
    std::shared_ptr<MysqlWrapper> get_mysql() { return _mysql; };
#endif
    std::shared_ptr<Server> get_http() { return _http_server; };
    std::shared_ptr<Scheduler> get_scheduler() { return _scheduler; };
    
    template <class T, typename... Args>
    void add_endpoint(const std::string& path, http::Method http_method,
                      Args &&...args) {
        auto handler = std::make_shared<T>(*this, std::forward<Args>(args)...);
        _request_handlers.push_back(handler);
        _http_server->add_endpoint(
            path, http_method,
            [handler](const http::RequestIncoming &request) -> http::Response {
                if (handler->get_sequire_handler()) {
                    auto sequire = handler->get_sequire_handler();
                    auto response = sequire(request);
                    if (response.code != 0)
                        return response;
                }
                return handler->handle(request);
            });
    }

    template <class T, typename... Args>
    void add_handler(http::Method http_method, Args &&...args) {
        auto handler = std::make_shared<T>(*this, std::forward<Args>(args)...);
        _request_handlers.push_back(handler);
        _http_server->add_handler(
            http_method,
            [handler](const http::RequestIncoming &request) -> http::Response {
                return handler->handle(request);
            });
    }

  private:
    asio::io_context _context;
#if SERVER_WEB_HAVE_MYSQLCONNECTOR == 1
    std::shared_ptr<MysqlWrapper> _mysql;
#endif
    std::shared_ptr<Server> _http_server;
    std::shared_ptr<Scheduler> _scheduler;
    std::shared_ptr<LRUCache> _lru_cache;

    std::list<std::shared_ptr<RequestHandler>> _request_handlers;
};

} // namespace http

#endif /* ServerApp_hpp */

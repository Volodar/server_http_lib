//
//  Exception.h
//  libEvent
//
//  Created by user-i157 on 14/02/17.
//  Copyright © 2017 user-i157. All rights reserved.
//

#ifndef HTTP_SERVER
#define HTTP_SERVER

#include "asio.hpp"
#include "asio/ssl.hpp"
#include "http_common.h"
#include "ssl_session.h"
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <thread>
#include <vector>

namespace http {

class Server {
  public:
    Server(asio::io_context &io_context, unsigned short http_port);
    Server(asio::io_context &io_context, unsigned short https_port, asio::ssl::context *ssl_context);
    Server(asio::io_context &io_context, unsigned short http_port, unsigned short https_port, asio::ssl::context *ssl_context);
    ~Server();

    void add_endpoint(const std::string& path, http::Method http_method,
                      Handler handler, Handler sequire_handler = nullptr);
    void add_handler(http::Method http_method, Handler handler);

    void run(int count_threads = -1);

  private:
    void accept_http();
    void accept_https();

  private:
    asio::io_context &_io_context;
    asio::ssl::context *_ssl_ctx;
    std::unique_ptr<asio::ip::tcp::acceptor> _http_acceptor;
    std::unique_ptr<asio::ip::tcp::acceptor> _https_acceptor;

    EndpointMap _endpoints;
    std::map<http::Method, Handler> _handlers;
    std::vector<std::thread> _threads;
};

} // namespace http

#endif /* HTTP_SERVER */

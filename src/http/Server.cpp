#include "Server.h"
#include "Scheduler.h"
#include "mysql_wrapper.h"
#include "utils.h"
#include <array>
#include <charconv>
#include <iostream>

namespace http {

template <typename SocketType>
void SslSession<SocketType>::do_handshake_or_read() {
    do_read(); // по умолчанию — HTTP
}

template <>
void SslSession<
    asio::ssl::stream<asio::ip::tcp::socket>>::do_handshake_or_read() {
    auto self = this->shared_from_this();
    _socket.async_handshake(asio::ssl::stream_base::server,
                            [this, self](const std::error_code &ec) {
                                if (!ec) {
                                    do_read();
                                } else {
                                    std::cerr << "SSL handshake error: "
                                              << ec.message() << std::endl;
                                }
                            });
}

Server::Server(asio::io_context &io_context, unsigned short http_port)
: _io_context(io_context)
, _ssl_ctx(nullptr)
, _http_acceptor(std::make_unique<asio::ip::tcp::acceptor>(_io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), http_port))) {
    
}

Server::Server(asio::io_context &io_context, unsigned short https_port, asio::ssl::context *ssl_context)
: _io_context(io_context)
, _ssl_ctx(ssl_context)
, _https_acceptor(std::make_unique<asio::ip::tcp::acceptor>(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), https_port))) {
    
}

Server::Server(asio::io_context &io_context, unsigned short http_port, unsigned short https_port, asio::ssl::context *ssl_context)
: _io_context(io_context)
, _ssl_ctx(ssl_context)
, _http_acceptor(std::make_unique<asio::ip::tcp::acceptor>(_io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), http_port)))
, _https_acceptor(std::make_unique<asio::ip::tcp::acceptor>(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), https_port))) {
    
}

Server::~Server() {
    for (auto &t : _threads) {
        t.join();
    }
}

void Server::add_endpoint(const std::string& path, http::Method http_method,
                          Handler handler, Handler sequere_handler) {
    EndpointKey key{path, http_method};
    _endpoints.emplace(key, std::make_pair(handler, sequere_handler));
}

void Server::add_handler(http::Method http_method, Handler handler) {
    _handlers[http_method] = handler;
}

void Server::run(int count_threads) {
    try {
        if (count_threads == -1)
            count_threads = std::thread::hardware_concurrency();

        if (_http_acceptor)
            accept_http();
        if (_https_acceptor)
            accept_https();

        std::cout << "Run http server with " << count_threads << " threads."
                  << std::endl;

        for (int i = 0; i < count_threads; ++i) {
            _threads.emplace_back([this]() { _io_context.run(); });
        }
    } catch (std::exception &e) {
        std::cerr << "Exception: on run server" << e.what() << std::endl;
    }
}

void Server::accept_http() {
    assert(_http_acceptor);
    _http_acceptor->async_accept(
        [this](std::error_code ec, asio::ip::tcp::socket socket) {
            if (!ec) {
                // Reduce latency: disable Nagle
                asio::ip::tcp::no_delay nd(true);
                std::error_code opt_ec;
                socket.set_option(nd, opt_ec);
                std::make_shared<SslSession<asio::ip::tcp::socket>>(
                    std::move(socket), &_endpoints, &_handlers)
                    ->start();
            }
            accept_http();
        });
}

void Server::accept_https() {
    assert(_ssl_ctx && _https_acceptor);
    _https_acceptor->async_accept([this](std::error_code ec,
                                         asio::ip::tcp::socket socket) {
        if (!ec) {
            auto ssl_stream = asio::ssl::stream<asio::ip::tcp::socket>(
                std::move(socket), *_ssl_ctx);
            // Reduce latency: disable Nagle on underlying socket
            std::error_code opt_ec;
            ssl_stream.lowest_layer().set_option(asio::ip::tcp::no_delay(true),
                                                 opt_ec);
            std::make_shared<
                SslSession<asio::ssl::stream<asio::ip::tcp::socket>>>(
                std::move(ssl_stream), &_endpoints, &_handlers)
                ->start();
        }
        accept_https();
    });
}

} // namespace http

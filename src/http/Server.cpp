#include "Server.h"
#include "Scheduler.h"
#include "mysql_wrapper.h"
#include "utils.h"
#include <array>
#include <charconv>
#include "Log.h"

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
                                } else if (is_noisy_ssl_handshake_error(ec)) {
                                    log_debug << "SSL handshake rejected: " << ec.message();
                                } else {
                                    log_warning << "SSL handshake error: " << ec.message();
                                }
                            });
}

Server::Server(asio::io_context &io_context, unsigned short http_port)
: _io_context(io_context)
, _ssl_ctx(nullptr)
, _http_acceptor(std::make_unique<asio::ip::tcp::acceptor>(_io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), http_port))) {
    log_info << "HTTP PORT:  " << http_port;
}

Server::Server(asio::io_context &io_context, unsigned short https_port, asio::ssl::context *ssl_context)
: _io_context(io_context)
, _ssl_ctx(ssl_context)
, _https_acceptor(std::make_unique<asio::ip::tcp::acceptor>(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), https_port))) {
    log_info << "HTTPS PORT:  " << https_port;
}

Server::Server(asio::io_context &io_context, unsigned short http_port, unsigned short https_port, asio::ssl::context *ssl_context)
: _io_context(io_context)
, _ssl_ctx(ssl_context)
, _http_acceptor(std::make_unique<asio::ip::tcp::acceptor>(_io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), http_port)))
, _https_acceptor(std::make_unique<asio::ip::tcp::acceptor>(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), https_port))) {
    log_info << "HTTP PORT:  " << http_port;
    log_info << "HTTPS PORT: " << https_port;
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

int Server::run(int count_threads) {
    try {
        if (count_threads == -1)
            count_threads = std::thread::hardware_concurrency();

        
        if (_http_acceptor)
            accept_http();
        if (_https_acceptor)
            accept_https();

        log_info << "Run http server with " << count_threads << " threads.";

        for (int i = 0; i < count_threads; ++i) {
            auto worker_id = i + 1;
            _threads.emplace_back([this, worker_id]() {
                Log::set_worker_id(worker_id);
                log_info << "Start http worker.";
                _io_context.run();
                log_info << "Stop http worker.";
                Log::reset_worker_id();
            });
        }
    } catch (std::exception &e) {
        log_error << "Exception: on run server" << e.what();
    }
    return count_threads;
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

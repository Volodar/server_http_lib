#include "ssl_session.h"
#include "http.h"
#include <charconv>
#include "Log.h"
#include "RequestIncoming.h"
#include "Exceptions.h"

namespace http {

template <typename SocketType>
void SslSession<SocketType>::process_request(std::string&& header_,
                                             std::string&& body_) {
    RequestIncoming request(std::move(header_));
    request.set_user_ip(_clientIP);
    request.set_data(std::move(body_));

    EndpointKey key{std::string(request.get_path()), request.get_method()};
    Response response = Response404;
    std::string statusText = "OK";
    auto it = _endpoints->find(key);
    log_debug << methodToStr(request.get_method()) << ": " << request.get_path() << "?" << request.get_params().to_string();
    if (it != _endpoints->end()) {
        try {
            auto sequire = it->second.second;
            if (sequire)
                response = sequire(request);
            if (response.code == ResponseNone.code || response.code == Response404.code)
                response = it->second.first(request);
        } catch (const ResponseException& e){
            response.code = e.get_code();
            response.body = e.get_body();
        } catch (const std::exception &e) {
            log_error << "Exception on SslSession::process_request, _endpoints" << e.what();
            response.code = 500;
            response.body = e.what();
        } catch (...) {
            log_error << "non std Exception on SslSession::process_request, _endpoints";
            response.code = 500;
            response.body = "non std Exception on SslSession::process_request, _endpoints";
        }
    } else {
        auto ith = _handlers->find(request.get_method());
        if (ith != _handlers->end()) {
            try {
                response = ith->second(request);
            } catch (const ResponseException& e){
                response.code = e.get_code();
                response.body = e.get_body();
            } catch (const std::exception &e) {
                log_error << "Exception on SslSession::process_request, _handlers" << e.what();
                response.code = 500;
                response.body = e.what();
            } catch (...) {
                log_error << "non std Exception on SslSession::process_request, _endpoints";
                response.code = 500;
                response.body = "non std Exception on SslSession::process_request, _endpoints";
            }
        }
    }

    auto connection = request.get_headers().get("Connection");
    _keep_alive = connection != "close" || connection == "keep-alive";

    _http_header = response.get_http_header(_keep_alive, _keep_alive_timeout);
    _http_body.clear();
    _http_body.swap(response.body);

    if(Log::Level::debug <= Log::get_level()){
        std::string_view sv_body = _http_body;
        std::string_view sv_header = _http_header;
        if(response.code == 300 || response.code == 302){
            auto k = sv_header.find('\n');
            if(k != std::string::npos)
                sv_body = sv_header.substr(k + 1);
        }
        size_t n = sv_body.find('\n');
        if(n == std::string::npos)
            n = 160;
        n = std::min<size_t>(sv_body.size(), n);
        log_debug << "    -> " << response.code << ((response.code == 300 || response.code == 302) ? ", first header: " : ": ") << sv_body.substr(0, n);
    }
    do_write();
}

template <> void SslSession<asio::ssl::stream<asio::ip::tcp::socket>>::do_write() {
    auto self = this->shared_from_this();
    std::array<asio::const_buffer, 2> bufs{asio::buffer(_http_header),
                                           asio::buffer(_http_body)};
    asio::async_write(_socket, bufs,
                      [this, self](std::error_code ec, std::size_t /*length*/) {
                          if (!_keep_alive) {
                              _socket.shutdown(ec);
                          } else {
                              cancel_keep_alive_timeout();
                              arm_keep_alive_timeout();
                              do_read();
                          }
                      });
}

template <> void SslSession<asio::ip::tcp::socket>::do_write() {
    auto self = this->shared_from_this();
    std::array<asio::const_buffer, 2> bufs{asio::buffer(_http_header),
                                           asio::buffer(_http_body)};
    asio::async_write(_socket, bufs,
                      [this, self](std::error_code ec, std::size_t /*length*/) {
                          if (!_keep_alive) {
                              _socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
                          } else {
                              cancel_keep_alive_timeout();
                              arm_keep_alive_timeout();
                              do_read();
                          }
                      });
}

} // namespace http

// Explicit instantiations to make template definitions linkable from other TUs
template void http::SslSession<asio::ip::tcp::socket>::process_request(
    std::string&& , std::string&& );
#if defined(ASIO_HAS_SSL)
template void http::SslSession<asio::ssl::stream<asio::ip::tcp::socket>>::process_request(
    std::string&& , std::string&& );
#endif

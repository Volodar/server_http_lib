//
//  ServerApp.cpp
//  dungeon2_server_analytics
//
//  Created by Vladimir Tolmachev on 14.04.2025.
//

#include "ServerApp.h"
#include "Log.h"

namespace http {

ServerApp::ServerApp(int http_port)
    : _context()
#if SERVER_WEB_HAVE_MYSQLCONNECTOR == 1
    , _mysql(std::make_shared<MysqlWrapper>())
#endif
      ,
      _http_server(std::make_shared<Server>(_context, http_port)),
      _scheduler(std::make_shared<Scheduler>(_context)) {
    connect_mysql();
}

ServerApp::ServerApp(int http_port, int https_port, asio::ssl::context *ssl_ctx)
    : _context()
#if SERVER_WEB_HAVE_MYSQLCONNECTOR == 1
    , _mysql(std::make_shared<MysqlWrapper>())
#endif
      ,
      _http_server(
          std::make_shared<Server>(_context, http_port, https_port, ssl_ctx)),
      _scheduler(std::make_shared<Scheduler>(_context)) {
    connect_mysql();
}

void ServerApp::connect_mysql() {
#if SERVER_WEB_HAVE_MYSQLCONNECTOR == 1
    auto mysql_host = std::getenv("MYSQL_HOST");
    auto mysql_database = std::getenv("MYSQL_DATABASE");
    auto mysql_user = std::getenv("MYSQL_USER");
    auto mysql_password = std::getenv("MYSQL_PASSWORD");

    if (mysql_host && mysql_user && mysql_password) {
        while (!MysqlWrapper::test_connection(mysql_host, mysql_user,
                                              mysql_password)) {
            log_error << "Mysql data base connection not ready. wait 1 seconds "
                         "and repeat...";
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        _mysql->connect(mysql_host, mysql_user, mysql_password);
        if (mysql_database != nullptr)
            _mysql->set_schema(mysql_database);
    } else {
        log_error << "Skip connect tp MySql: env is empty";
    }
#endif
}

void ServerApp::run(int num_threads) {
    _http_server->run(num_threads);
    _context.run();
}

void ServerApp::set_lru_cache(size_t capacity_mb){
    _lru_cache = std::make_shared<LRUCache>(capacity_mb);
}
std::shared_ptr<LRUCache> ServerApp::get_lru_cache(){
    return _lru_cache;
}

} // namespace http

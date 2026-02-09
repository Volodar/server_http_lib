#include "postresql_wrapper.h"

#include "Log.h"

#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

namespace http {

PostresqlWrapper::PostresqlWrapper() {}

PostresqlWrapper::~PostresqlWrapper() {
    std::unique_lock<std::mutex> lock(_mutex);
    while (!_connections.empty()) {
        auto conn = _connections.front();
        _connections.pop_back();
        delete conn;
    }
}

bool PostresqlWrapper::test_connection(const std::string& host,
                                       const std::string& login,
                                       const std::string& password,
                                       const std::string& dbname) {
    bool result = false;
    try {
        std::ostringstream conn_string("");
        conn_string << "host=" << host << " user=" << login
        << " password=" << password << " dbname=" << dbname;
        pqxx::connection conn(conn_string.str());
        result = conn.is_open();
    } catch (const pqxx::broken_connection& e) {
        log_debug << e.what();
    }
    return result;
}

void PostresqlWrapper::connect(const std::string& host,
                               const std::string& login,
                               const std::string& password,
                               const std::string& dbname) {
    _host = host;
    _user = login;
    _password = password;
    _schema = dbname;
    
    std::ostringstream conn_string("");
    conn_string << "host=" << host << " user=" << login << " password="
    << password << " dbname=" << dbname;
    auto conn_str = conn_string.str();
    try {
        auto count = std::thread::hardware_concurrency();
        auto mysql_connections_count = std::getenv("MYSQL_CONN");
        if (mysql_connections_count) {
            count = std::stoi(mysql_connections_count);
        }
        
        log_info << "Count mysql connections=" << count;
        for (int i = 0; i < static_cast<int>(count); ++i) {
            _connections.push_back(new pqxx::connection(conn_str));
        }
        
    } catch (const pqxx::broken_connection& e) {
        std::cout << "Failed to establish connection: " << e.what() << std::endl;
    }
}

void PostresqlWrapper::reconnect() {
    std::unique_lock<std::mutex> lock(_mutex);
    log_error << "PostresqlWrapper::reconnect: очищаем старые соединения...";
    for (auto conn : _connections) {
        try {
            if (conn->is_open()) {
                conn->close();
            }
        } catch (...) {
        }
        delete conn;
    }
    _connections.clear();
    connect(_host, _user, _password, _schema);
    _condition.notify_all();
}

void PostresqlWrapper::set_schema(const std::string& schema) {
    _schema = schema;
}

bool PostresqlWrapper::query(const std::string& query) {
    try {
        auto connection = get_connection();
        std::unique_lock<std::mutex> lock(_mutex);
        pqxx::work work(*connection, "query");
        work.exec(query);
        work.commit();
        return true;
    } catch (const pqxx::sql_error&) {
        return false;
    }
}

std::unique_ptr<SqlResult> PostresqlWrapper::query_get(
                                                       const std::string& query) {
    try {
        auto connection = get_connection();
        std::unique_lock<std::mutex> lock(_mutex);
        pqxx::work work(*connection, "query");
        auto res = work.exec(query);
        work.commit();
        return std::make_unique<PqResult>(std::move(res));
    } catch (const pqxx::sql_error&) {
        return std::make_unique<PqResult>(pqxx::result());
    }
}

std::shared_ptr<pqxx::connection> PostresqlWrapper::get_connection() {
    std::unique_lock<std::mutex> lock(_mutex);
    if (!_condition.wait_for(lock, std::chrono::seconds(5),
                             [this]() { return !_connections.empty(); })) {
        reconnect();
        throw std::exception();
    }
    auto conn = _connections.back();
    _connections.pop_back();
    return std::shared_ptr<pqxx::connection>(
                                             conn, [this](pqxx::connection* c) { release_connection(c); });
}

void PostresqlWrapper::release_connection(pqxx::connection* conn) {
    std::unique_lock<std::mutex> lock(_mutex);
    _connections.push_back(conn);
    lock.unlock();
    _condition.notify_one();
}

} // namespace http

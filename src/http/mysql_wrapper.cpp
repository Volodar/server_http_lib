#if SERVER_WEB_HAVE_MYSQLCONNECTOR == 1

#include "mysql_wrapper.h"

#include "Log.h"

#include <chrono>
#include <cppconn/exception.h>
#include <cppconn/statement.h>
#include <thread>

namespace http {

namespace {

enum class MySqlErrorCode : int {
    ServerGone = 2006,
    ServerLost = 2013,
    TimedOut = 2001,
};

} // namespace

MysqlWrapper::MysqlWrapper() : _driver(nullptr) {}

MysqlWrapper::~MysqlWrapper() {
    std::unique_lock<std::mutex> lock(_mutex);
    while (!_connections.empty()) {
        sql::Connection* conn = _connections.front();
        _connections.pop_back();
        delete conn;
    }
}

bool MysqlWrapper::test_connection(const std::string& host,
                                   const std::string& login,
                                   const std::string& password) {
    bool result = false;
    try {
        auto driver = get_driver_instance();
        sql::Connection* conn = driver->connect(host, login, password);
        result = true;
        delete conn;
    } catch (sql::SQLException& e) {
        log_debug << e.what();
    }
    return result;
}

void MysqlWrapper::connect(const std::string& host, const std::string& login,
                           const std::string& password,
                           const std::string& schema) {
    _host = host;
    _user = login;
    _password = password;
    _schema = schema;
    try {
        log_info << "MysqlWrapper::connecting...";
        _driver = get_driver_instance();
        
        auto count = std::thread::hardware_concurrency();
        auto mysql_connections_count = std::getenv("MYSQL_CONN");
        if (mysql_connections_count) {
            count = std::stoi(mysql_connections_count);
        }
        
        log_info << "Count mysql connections=" << count;
        for (int i = 0; i < static_cast<int>(count); ++i) {
            try {
                sql::Connection* conn = _driver->connect(host, login, password);
                _connections.push_back(conn);
            } catch (sql::SQLException& e) {
                log_error << "Error on connection to mysql: " << e.what();
            }
        }
        if (!_schema.empty()) {
            set_schema(_schema);
        }
        _condition.notify_all();
    } catch (const sql::SQLException& e) {
        log_error << "SQLException: " << e.what();
    }
}

void MysqlWrapper::reconnect() {
    {
        std::unique_lock<std::mutex> lock(_mutex);
        log_error << "MysqlWrapper::reconnect: очищаем старые соединения...";
        for (auto conn : _connections) {
            try {
                if (!conn->isClosed()) {
                    conn->close();
                }
            } catch (...) {
            }
            delete conn;
        }
        _connections.clear();
        connect(_host, _user, _password, "");
    }
    if (!_schema.empty()) {
        set_schema(_schema);
    }
    _condition.notify_all();
}

void MysqlWrapper::set_schema(const std::string& schema) {
    try {
        query("CREATE SCHEMA IF NOT EXISTS " + schema);
        _schema = schema;
        for (auto& connection : _connections) {
            connection->setSchema(schema);
        }
    } catch (const sql::SQLException& e) {
        log_error << "SQLException: " << e.what();
        log_error << "Connect to schema: " << schema;
    }
}

bool MysqlWrapper::query(const std::string& query) {
    try {
        auto conn = get_connection();
        std::unique_ptr<sql::Statement> stmt(conn->createStatement());
        stmt->execute(query);
        return true;
    } catch (const sql::SQLException& e) {
        log_error << "SQLException: " << e.what() << "\nQuery: " << query;
        if (e.getErrorCode() == static_cast<int>(MySqlErrorCode::ServerLost) ||
            e.getErrorCode() == static_cast<int>(MySqlErrorCode::ServerGone) ||
            e.getErrorCode() == static_cast<int>(MySqlErrorCode::TimedOut)) {
            reconnect();
            this->query(query);
        }
        return false;
    }
}

std::unique_ptr<SqlResult> MysqlWrapper::query_get(const std::string& query) {
    try {
        auto conn = get_connection();
        std::unique_ptr<sql::Statement> stmt(conn->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(query));
        return std::make_unique<MysqlResult>(std::move(res));
    } catch (const sql::SQLException& e) {
        log_error << "SQLException: " << e.what();
        log_error << "Query: " << query;
        if (e.getErrorCode() == static_cast<int>(MySqlErrorCode::ServerLost) ||
            e.getErrorCode() == static_cast<int>(MySqlErrorCode::ServerGone) ||
            e.getErrorCode() == static_cast<int>(MySqlErrorCode::TimedOut)) {
            reconnect();
            return query_get(query);
        }
        return std::make_unique<MysqlResult>(nullptr);
    }
}

std::shared_ptr<sql::Connection> MysqlWrapper::get_connection() {
    std::unique_lock<std::mutex> lock(_mutex);
    if (!_condition.wait_for(lock, std::chrono::seconds(5),
                             [this]() { return !_connections.empty(); })) {
        throw sql::SQLException("Timeout waiting for MySQL connection", "",
                                static_cast<int>(MySqlErrorCode::TimedOut));
    }
    sql::Connection* conn = _connections.back();
    _connections.pop_back();
    return std::shared_ptr<sql::Connection>(
                                            conn, [this](sql::Connection* c) { release_connection(c); });
}

void MysqlWrapper::release_connection(sql::Connection* conn) {
    std::unique_lock<std::mutex> lock(_mutex);
    _connections.push_back(conn);
    lock.unlock();
    _condition.notify_one();
}

} // namespace http

#endif

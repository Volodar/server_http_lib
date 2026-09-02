#if SERVER_WEB_HAVE_MYSQLCONNECTOR == 1

#include "mysql_wrapper.h"
#include "utils.h"
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <thread>
#include <chrono>
#include "Log.h"

enum class MySqlErrorCode : int {
    // Общие ошибки
    ServerGone = 2006, // MySQL server has gone away
    ServerLost = 2013, // Lost connection to MySQL server during query
    CommandsOutOfSync = 2014, // Commands out of sync; you can't run this command now
    TooManyConnections = 1040, // Too many connections
    LockWaitTimeout = 1205,    // Lock wait timeout exceeded
    DeadlockFound = 1213,      // Deadlock found when trying to get lock
    PacketTooLarge = 1153,     // Got a packet bigger than 'max_allowed_packet'

    // Аутентификация / доступ
    AccessDenied = 1045,    // Access denied for user
    UnknownDatabase = 1049, // Unknown database
    UnknownHost = 2005,     // Unknown MySQL server host
    WrongHostInfo = 2002,   // Can't connect to local MySQL server

    // Ошибки SQL
    SyntaxError = 1064,         // You have an error in your SQL syntax
    TableDoesNotExist = 1146,   // Table doesn't exist
    ColumnDoesNotExist = 1054,  // Unknown column
    DuplicateEntry = 1062,      // Duplicate entry for key
    TruncatedWrongValue = 1366, // Incorrect value for column
    InactivityTimeout = 4031,   // Client disconnected due to inactivity

    // Ошибки транзакций
    ReadOnlyTransaction = 1290, // Read-only mode
    TransactionAborted = 1196, // Warning: Some non-transactional changed tables
                               // couldn't be rolled back

    // Неизвестная ошибка
    Unknown = 0,
    
    //User:
    TimedOut = 2001,
};

MysqlWrapper::MysqlWrapper() : _driver(nullptr) {}

MysqlWrapper::~MysqlWrapper() {
    std::unique_lock<std::mutex> lock(_mutex);
    while (!_connections.empty()) {
        sql::Connection *conn = _connections.front().connection;
        _connections.pop_front();
        delete conn;
    }
}

bool MysqlWrapper::test_connection(const std::string& host,
                                   const std::string& login,
                                   const std::string& password) {
    bool result = false;
    try {
        auto driver = get_driver_instance();
        sql::Connection *conn = driver->connect(host, login, password);
        result = true;
        delete conn;
    } catch (sql::SQLException &e) {
        log_debug << e.what();
    }
    return result;
}

void MysqlWrapper::connect(const std::string& host, const std::string& login,
                           const std::string& password) {
    _host = host;
    _user = login;
    _password = password;
    try {
        log_info << "MysqlWrapper::connecting...";
        _driver = get_driver_instance();
        
        auto count = std::thread::hardware_concurrency();
        auto mysql_connections_count = std::getenv("MYSQL_CONN");
        if(mysql_connections_count){
            count = std::stoi(mysql_connections_count);
        }
        
        log_info << "Count mysql connections=" << count;
        auto generation = _generation.load(std::memory_order_acquire);
        for (int i = 0; i < count; ++i) {
            try {
                sql::Connection *conn = create_connection();
                _connections.push_back({conn, generation});
            } catch (sql::SQLException &e) {
                log_error << "Error on connection to mysql: " << e.what();
            }
        }
        _condition.notify_all();
    } catch (const sql::SQLException &e) {
        log_error << "SQLException: " << e.what();
        return;
    }
}

void MysqlWrapper::reconnect() {
    std::unique_lock<std::mutex> lock(_mutex);
    log_error << "MysqlWrapper::reconnect: очищаем старые соединения...";
    _generation.fetch_add(1, std::memory_order_release);
    for (const auto &pooled : _connections) {
        auto conn = pooled.connection;
        try {
            if (!conn->isClosed()) {
                conn->close();
            }
        } catch (...) {
        }
        delete conn;
    }
    _connections.clear();
    connect(_host, _user, _password);
    lock.unlock();
    if (!_schema.empty())
        set_schema(_schema);
    // Разбудим ожидающие потоки, если появились новые соединения
    _condition.notify_all();
}

void MysqlWrapper::set_schema(const std::string& schema) {
    try {
        query("CREATE SCHEMA IF NOT EXISTS " + schema);
        _schema = schema;
        for (auto &pooled : _connections)
            pooled.connection->setSchema(schema);
    } catch (const sql::SQLException &e) {
        log_error << "SQLException: " << e.what();
        log_error << "Connect to schema: " << schema;
        return;
    }
}

bool MysqlWrapper::has_table(const std::string& schema, const std::string& table_name){
    auto result = query_get("SHOW TABLES FROM " + schema);
    while (result->next()) {
        if (result->getString(1) == table_name)
            return true;
    }
    return false;
}

void MysqlWrapper::create_table(const std::string& schema,
                                const std::string& table,
                                const std::string& source) {
    if(has_table(schema, table))
        return;
    
    auto queries = http::split(source, ';');
    for (auto &query : queries) {
        http::strip(query);
        if (!query.empty())
            this->query(build_query(query, schema, table));
    }
}

void MysqlWrapper::alter_table_add_column(const std::string& schema,
                                          const std::string& table,
                                          const std::string& column,
                                          const std::string& column_type) {
    bool tableExist = has_table(schema, table);
    if (!tableExist)
        throw sql::SQLException("table " + schema + "." + table +
                                "not exist. Cannot add column:" + column);

    auto query = build_query(
        "SELECT COUNT(*) AS cnt FROM information_schema.COLUMNS WHERE "
        "TABLE_SCHEMA = '$0' AND TABLE_NAME = '$1' AND COLUMN_NAME = '$2'",
        schema, table, column);
    auto result = query_get(query);
    while (result->next()) {
        if (result->getInt(1) > 0)
            return;
    }
    auto source = build_query("ALTER TABLE $0.$1 ADD COLUMN $2 $3;", schema, table, column, column_type);
    http::replace(source, "\\\"", "\"");
    if(!this->query(source)) {
        throw sql::SQLException("Cannot add column " + column + " to table " + schema + "." + table);
    }
}

void MysqlWrapper::alter_table_change_column(const std::string& schema,
                                             const std::string& table,
                                             const std::string& column,
                                             const std::string& column_type) {
    
    auto result = query_get(build_query(R"(SELECT COLUMN_NAME, COLUMN_TYPE
      FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = '$0'
        AND TABLE_NAME = '$1'
        AND COLUMN_NAME = '$2')", schema, table, column));
    
    while (result && result->next()) {
        if(result->getString(1) == column) {
            std::string type = result->getString(2);
            if(type == column_type){
                return;
            }
        }
    }
    auto source = build_query("ALTER TABLE $0.$1 MODIFY COLUMN $2 $3", schema, table, column, column_type);
    http::replace(source, "\\\"", "\"");
    this->query(source);
}

bool MysqlWrapper::has_index(const std::string& schema, const std::string& table, const std::string& index) {
    if(!has_table(schema, table)) {
        return false;
    }
    auto result = query_get(build_query("SHOW INDEX FROM `$1` FROM `$0`", schema, table));
    while(result->next()) {
        if(result->getString(3) == index) {
            return true;
        }
    }
    return false;
}

void MysqlWrapper::create_index(const std::string& schema,
                                const std::string& table,
                                const std::string& index,
                                const std::string& source) {
    if(!has_table(schema, table)) {
        throw sql::SQLException("table " + schema + "." + table + " not exist. Cannot create index with query: " + source);
    }
    if(has_index(schema, table, index)) {
        return;
    }

    auto query = source;
    if(query.empty()) {
        query = build_query("CREATE INDEX idx_$2 ON $0.$1 ($2);", schema, table, index);
    }
    if(!this->query(query)) {
        throw sql::SQLException("Cannot create index " + index + " on table " + schema + "." + table);
    }
}

bool MysqlWrapper::query(const std::string& query) {
    for (int attempt = 0; attempt < 2; ++attempt) {
        try {
            auto conn = get_connection();
            std::unique_ptr<sql::Statement> stmt(conn->createStatement());
            stmt->execute(query);
            return true;
        } catch (const sql::SQLException &e) {
            log_error << "SQLException: " << e.what() << "\nQuery: " << query;
            if (attempt == 0 && should_retry(e)) {
                reconnect();
                continue;
            }
            return false;
        }
    }
    return false;
}
std::unique_ptr<sql::ResultSet>
MysqlWrapper::query_get(const std::string& query) {
    for (int attempt = 0; attempt < 2; ++attempt) {
        try {
            auto conn = get_connection();
            std::unique_ptr<sql::Statement> stmt(conn->createStatement());
            std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(query));
            return res;
        } catch (const sql::SQLException &e) {
            log_error << "SQLException: " << e.what();
            log_error << "Query: " << query;
            if (attempt == 0 && should_retry(e)) {
                reconnect();
                continue;
            }
            return nullptr;
        }
    }
    return nullptr;
}

std::shared_ptr<sql::Connection> MysqlWrapper::get_connection() {
    std::unique_lock<std::mutex> lock(_mutex);
    if (!_condition.wait_for(lock, std::chrono::seconds(5), [this]() { return !_connections.empty(); })) {
        lock.unlock();
        auto connection = create_connection();
        auto generation = _generation.load(std::memory_order_acquire);
        log_info << "MysqlWrapper::get_connection: empty pool replenished";
        return std::shared_ptr<sql::Connection>(connection, [this, generation](sql::Connection *conn) {
            this->release_connection(conn, generation);
        });
    }
    auto pooled = _connections.back();
    _connections.pop_back();
    lock.unlock();

    try {
        auto is_closed = pooled.connection->isClosed();
        auto is_valid = !is_closed && pooled.connection->isValid();
        if (!should_replace_connection(is_closed, is_valid)) {
            return std::shared_ptr<sql::Connection>(pooled.connection, [this, generation = pooled.generation](sql::Connection *conn) {
                this->release_connection(conn, generation);
            });
        }
    } catch (const sql::SQLException &e) {
        log_error << "MysqlWrapper::get_connection: stale connection: " << e.what();
    }

    try {
        if (!pooled.connection->isClosed()) {
            pooled.connection->close();
        }
    } catch (...) {
    }
    delete pooled.connection;

    auto connection = create_connection();
    auto generation = _generation.load(std::memory_order_acquire);
    log_info << "MysqlWrapper::get_connection: stale connection replaced";
    return std::shared_ptr<sql::Connection>(connection, [this, generation](sql::Connection *conn) {
        this->release_connection(conn, generation);
    });
}

sql::Connection *MysqlWrapper::create_connection() {
    auto connection = _driver->connect(_host, _user, _password);
    if (!_schema.empty()) {
        connection->setSchema(_schema);
    }
    return connection;
}

bool MysqlWrapper::should_replace_connection(bool is_closed, bool is_valid) {
    return is_closed || !is_valid;
}

void MysqlWrapper::release_connection(sql::Connection *conn, uint64_t generation) {
    if (!conn) {
        return;
    }
    std::unique_lock<std::mutex> lock(_mutex);
    auto current_generation = _generation.load(std::memory_order_acquire);
    if (generation != current_generation) {
        lock.unlock();
        try {
            if (!conn->isClosed()) {
                conn->close();
            }
        } catch (...) {
        }
        delete conn;
        return;
    }
    _connections.push_back({conn, generation});
    lock.unlock();
    _condition.notify_one();
}

bool MysqlWrapper::should_retry(const sql::SQLException& e) const {
    auto code = e.getErrorCode();
    return code == static_cast<int>(MySqlErrorCode::ServerLost) ||
           code == static_cast<int>(MySqlErrorCode::ServerGone) ||
           code == static_cast<int>(MySqlErrorCode::TimedOut) ||
           code == static_cast<int>(MySqlErrorCode::InactivityTimeout);
}

#endif

#ifndef MYSQL_WRAPPER_H
#define MYSQL_WRAPPER_H

#if SERVER_WEB_HAVE_MYSQLCONNECTOR == 1

#include <cassert>
#include <condition_variable>
#include <cppconn/driver.h>
#include <cppconn/resultset.h>
#include <memory>
#include <atomic>
#include <queue>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include "utils.h"

namespace sql {
class SQLException;
}

class MysqlWrapper {
  public:
    MysqlWrapper();
    ~MysqlWrapper();
    static bool test_connection(const std::string& host,
                                const std::string& login,
                                const std::string& password);
    void connect(const std::string& host, const std::string& login,
                 const std::string& password);
    void reconnect();
    void set_schema(const std::string& schema);
    
    bool has_table(const std::string& schema, const std::string& table_name);
    void create_table(const std::string& schema, const std::string& table,
                      const std::string& source);
    void alter_table_add_column(const std::string& schema,
                                const std::string& table,
                                const std::string& column,
                                const std::string& column_type);
    void alter_table_change_column(const std::string& schema,
                                   const std::string& table,
                                   const std::string& column,
                                   const std::string& column_type);
    bool has_index(const std::string& schema, const std::string& table, const std::string& index);
    void create_index(const std::string& schema, const std::string& table,
                      const std::string& index, const std::string& source = "");

    bool query(const std::string& query);
    std::unique_ptr<sql::ResultSet> query_get(const std::string& query);

    std::shared_ptr<sql::Connection> get_connection();

  protected:
    void release_connection(sql::Connection *conn, uint64_t generation);
    bool should_retry(const sql::SQLException& e) const;

  private:
    struct PooledConnection {
        sql::Connection *connection = nullptr;
        uint64_t generation = 0;
    };

    sql::Driver *_driver;

    std::string _host;
    std::string _user;
    std::string _password;
    std::string _schema;
    std::deque<PooledConnection> _connections;
    std::atomic<uint64_t> _generation{1};
    std::mutex _mutex;
    std::condition_variable _condition;
};

template <typename T> std::string to_sql_value(const T &value) {
    return std::to_string(value);
}
template <typename T> std::string to_sql_raw_value(const T &value) {
    return std::to_string(value);
}
inline std::string to_sql_value(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() * 2);
    for(char c : value){
        if(c == '\''){
            escaped.push_back('\'');
            escaped.push_back('\'');
            continue;
        }
        if(c == '\\' || c == '"'){
            escaped.push_back('\\');
            escaped.push_back(c);
            continue;
        }
        escaped.push_back(c);
    }
    return escaped;
}
inline std::string to_sql_raw_value(const std::string& value) {
    return value;
}
inline std::string to_sql_value(std::string_view value) {
    return to_sql_value(std::string(value));
}
inline std::string to_sql_raw_value(std::string_view value) {
    return std::string(value);
}
inline std::string to_sql_value(const char *value) {
    return to_sql_value(std::string(value));
}
inline std::string to_sql_raw_value(const char *value) {
    return std::string(value);
}
inline std::string to_sql_value(char *value) {
    return to_sql_value(std::string(value));
}
inline std::string to_sql_raw_value(char *value) {
    return std::string(value);
}

inline bool is_inside_sql_string_literal(const std::string& query, size_t pos) {
    bool in_single_quote = false;
    bool in_double_quote = false;
    size_t i = 0;
    while (i < pos && i < query.size()) {
        char ch = query[i];
        if (!in_double_quote && ch == '\'') {
            if (in_single_quote) {
                if ((i + 1) < pos && query[i + 1] == '\'') {
                    i += 2;
                    continue;
                }
                in_single_quote = false;
                ++i;
                continue;
            }
            in_single_quote = true;
            ++i;
            continue;
        }
        if (!in_single_quote && ch == '"') {
            if (in_double_quote) {
                if ((i + 1) < pos && query[i + 1] == '"') {
                    i += 2;
                    continue;
                }
                in_double_quote = false;
                ++i;
                continue;
            }
            in_double_quote = true;
            ++i;
            continue;
        }
        if (ch == '\\' && (in_single_quote || in_double_quote)) {
            i += 2;
            continue;
        }
        ++i;
    }
    return in_single_quote || in_double_quote;
}

template <typename... Args>
std::string build_query(const std::string& template_str, Args &&...args) {
    std::string query = template_str;
    std::tuple<Args...> values(std::forward<Args>(args)...);

    constexpr int arg_count = sizeof...(Args);

    for (int i = arg_count - 1; i >= 0; --i) {
        std::string token = "$" + std::to_string(i);
        std::string escaped_value;
        std::string raw_value;

        std::apply(
            [&](const auto &...unpacked) {
                int index = 0;
                ((index++ == i ? escaped_value = to_sql_value(unpacked)
                               : std::string()),
                 ...);
            },
            values);
        std::apply(
            [&](const auto &...unpacked) {
                int index = 0;
                ((index++ == i ? raw_value = to_sql_raw_value(unpacked)
                               : std::string()),
                 ...);
            },
            values);

        size_t pos = 0;
        bool was_replace = false;
        while ((pos = query.find(token, pos)) != std::string::npos) {
            bool quoted_context = is_inside_sql_string_literal(query, pos);
            const std::string& value = quoted_context ? escaped_value : raw_value;
            query.replace(pos, token.length(), value);
            pos += value.length();
            was_replace = true;
        }
        (void)was_replace;
        assert(was_replace && "missing placeholder in query");
    }

    assert(query.find("$") == std::string::npos && "unresolved placeholders remain");
    return query;
}

#endif

#endif

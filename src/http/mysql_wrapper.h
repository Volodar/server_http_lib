#ifndef MYSQL_WRAPPER_H
#define MYSQL_WRAPPER_H

#if SERVER_WEB_HAVE_MYSQLCONNECTOR == 1

#include <cassert>
#include <condition_variable>
#include <cppconn/driver.h>
#include <cppconn/resultset.h>
#include <memory>
#include <queue>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>

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
    void create_index(const std::string& schema, const std::string& table,
                      const std::string& index, const std::string& source = "");

    bool query(const std::string& query);
    std::unique_ptr<sql::ResultSet> query_get(const std::string& query);

    std::shared_ptr<sql::Connection> get_connection();

  protected:
    void release_connection(sql::Connection *conn);

  private:
    sql::Driver *_driver;

    std::string _host;
    std::string _user;
    std::string _password;
    std::string _schema;
    std::deque<sql::Connection *> _connections;
    std::mutex _mutex;
    std::condition_variable _condition;
};

template <typename T> std::string to_sql_value(const T &value) {
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
inline std::string to_sql_value(std::string_view value) {
    return to_sql_value(std::string(value));
}
inline std::string to_sql_value(const char *value) {
    return to_sql_value(std::string(value));
}
inline std::string to_sql_value(char *value) {
    return to_sql_value(std::string(value));
}

template <typename... Args>
std::string build_query(const std::string& template_str, Args &&...args) {
    std::string query = template_str;
    std::tuple<Args...> values(std::forward<Args>(args)...);

    constexpr int arg_count = sizeof...(Args);

    for (int i = arg_count - 1; i >= 0; --i) {
        std::string token = "$" + std::to_string(i);
        std::string value;

        std::apply(
            [&](const auto &...unpacked) {
                int index = 0;
                ((index++ == i ? value = to_sql_value(unpacked)
                               : std::string()),
                 ...);
            },
            values);

        size_t pos = 0;
        bool was_replace = false;
        while ((pos = query.find(token, pos)) != std::string::npos) {
            query.replace(pos, token.length(), value);
            pos += value.length();
            was_replace = true;
        }

        assert(was_replace && "missing placeholder in query");
    }

    assert(query.find("$") == std::string::npos &&
           "unresolved placeholders remain");
    return query;
}

#endif

#endif

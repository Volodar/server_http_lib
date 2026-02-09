#include "sql_wrapper.h"

#include "sql.h"
#include "utils.h"

#if SERVER_WEB_HAVE_MYSQLCONNECTOR == 1
#include <cppconn/exception.h>
#endif
#include <stdexcept>

namespace http {

bool SqlWrapperBase::has_table(const std::string& schema,
                               const std::string& table_name) {
    auto result = query_get("SHOW TABLES FROM " + schema);
    while (result->next()) {
        if (result->get_string(1) == table_name) {
            return true;
        }
    }
    return false;
}

void SqlWrapperBase::create_table(const std::string& schema,
                                  const std::string& table,
                                  const std::string& source) {
    if (has_table(schema, table)) {
        return;
    }
    
    auto queries = http::split(source, ';');
    for (auto& query_part : queries) {
        http::strip(query_part);
        if (!query_part.empty()) {
            query(build_query(query_part, schema, table));
        }
    }
}

void SqlWrapperBase::alter_table_add_column(const std::string& schema,
                                            const std::string& table,
                                            const std::string& column,
                                            const std::string& column_type) {
    assert(column_type.find("DEFAULT") != std::string::npos);
    
    if (!has_table(schema, table)) {
#if SERVER_WEB_HAVE_MYSQLCONNECTOR == 1
        throw sql::SQLException("table " + schema + "." + table +
                                "not exist. Cannot add column:" + column);
#else
        throw std::runtime_error("table " + schema + "." + table +
                                 "not exist. Cannot add column:" + column);
#endif
    }
    
    auto result = query_get(build_query(
                                        "SELECT COUNT(*) AS cnt FROM information_schema.COLUMNS WHERE "
                                        "TABLE_SCHEMA = '$0' AND TABLE_NAME = '$1' AND COLUMN_NAME = '$2'",
                                        schema, table, column));
    while (result->next()) {
        if (result->get_int(1) > 0) {
            return;
        }
    }
    
    query(build_query("ALTER TABLE $0.$1 ADD COLUMN $2 $3;", schema, table,
                      column, column_type));
}

void SqlWrapperBase::alter_table_change_column(const std::string& schema,
                                               const std::string& table,
                                               const std::string& column,
                                               const std::string& column_type) {
    auto result = query_get(build_query(R"(SELECT COLUMN_NAME, COLUMN_TYPE
        FROM INFORMATION_SCHEMA.COLUMNS
        WHERE TABLE_SCHEMA = '$0'
        AND TABLE_NAME = '$1'
        AND COLUMN_NAME = '$2')",
                                        schema, table, column));
    
    while (result->next()) {
        if (result->get_string(1) == column) {
            if (result->get_string(2) == column_type) {
                return;
            }
        }
    }
    
    query(build_query("ALTER TABLE $0.$1 MODIFY COLUMN $2 $3", schema, table,
                      column, column_type));
}

void SqlWrapperBase::create_index(const std::string& schema,
                                  const std::string& table,
                                  const std::string& index,
                                  const std::string& source) {
    if (!has_table(schema, table)) {
#if SERVER_WEB_HAVE_MYSQLCONNECTOR == 1
        throw sql::SQLException("table " + schema + "." + table +
                                "not exist. Cannot create index with query: " +
                                source);
#else
        throw std::runtime_error("table " + schema + "." + table +
                                 "not exist. Cannot create index with query: " +
                                 source);
#endif
    }
    
    auto result = query_get(build_query("SHOW INDEX FROM `$1` FROM `$0`", schema,
                                        table));
    while (result->next()) {
        if (result->get_string(5) == index) {
            return;
        }
    }
    
    if (source.empty()) {
        query(build_query("CREATE INDEX idx_$2 ON $0.$1 ($2);", schema, table,
                          index));
        return;
    }
    query(source);
}

} // namespace http

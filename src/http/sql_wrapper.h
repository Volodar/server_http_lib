#pragma once

#include "sql_result.h"

#include <cassert>
#include <memory>
#include <string>

namespace http {

class SqlWrapper {
public:
    SqlWrapper() = default;
    virtual ~SqlWrapper() = default;
    
    virtual void connect(const std::string& host, const std::string& login,
                         const std::string& password,
                         const std::string& schema) = 0;
    virtual void reconnect() = 0;
    virtual void set_schema(const std::string& schema) = 0;
    virtual bool has_table(const std::string& schema,
                           const std::string& table_name) = 0;
    virtual void create_table(const std::string& schema, const std::string& table,
                              const std::string& source) = 0;
    virtual void alter_table_add_column(const std::string& schema,
                                        const std::string& table,
                                        const std::string& column,
                                        const std::string& column_type) = 0;
    virtual void alter_table_change_column(const std::string& schema,
                                           const std::string& table,
                                           const std::string& column,
                                           const std::string& column_type) = 0;
    virtual void create_index(const std::string& schema, const std::string& table,
                              const std::string& index,
                              const std::string& source = "") = 0;
    virtual bool query(const std::string& query) = 0;
    virtual std::unique_ptr<SqlResult> query_get(const std::string& query) = 0;
};

class SqlWrapperBase : public SqlWrapper {
public:
    bool has_table(const std::string& schema,
                   const std::string& table_name) override;
    
    void create_table(const std::string& schema, const std::string& table,
                      const std::string& source) override;
    
    void alter_table_add_column(const std::string& schema,
                                const std::string& table,
                                const std::string& column,
                                const std::string& column_type) override;
    
    void alter_table_change_column(const std::string& schema,
                                   const std::string& table,
                                   const std::string& column,
                                   const std::string& column_type) override;
    
    void create_index(const std::string& schema, const std::string& table,
                      const std::string& index,
                      const std::string& source = "") override;
};

} // namespace http

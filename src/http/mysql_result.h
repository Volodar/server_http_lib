#pragma once

#if SERVER_WEB_HAVE_MYSQLCONNECTOR == 1

#include "sql_result.h"

#include <cppconn/resultset.h>
#include <memory>
#include <string>

namespace http {

class MysqlResult : public SqlResult {
public:
    explicit MysqlResult(std::unique_ptr<sql::ResultSet>&& raw) noexcept;
    MysqlResult(MysqlResult&& raw) noexcept;
    MysqlResult& operator=(MysqlResult&& raw) noexcept;
    
    operator bool() const { return _raw != nullptr; }
    
    bool next() override;
    int get_int(int index) override;
    float get_float(int index) override;
    double get_double(int index) override;
    std::string get_string(int index) override;
    
private:
    std::unique_ptr<sql::ResultSet> _raw;
};

} // namespace http

#endif

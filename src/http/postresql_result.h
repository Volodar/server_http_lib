#pragma once

#include "pqxx/result"
#include "sql_result.h"

#include <string>

namespace http {

class PqResult : public SqlResult {
public:
    explicit PqResult(pqxx::result&& raw) noexcept;
    PqResult(PqResult&& raw) noexcept;
    PqResult& operator=(PqResult&& raw) noexcept;
    
    operator bool() const;
    
    bool next() override;
    int get_int(int index) override;
    float get_float(int index) override;
    double get_double(int index) override;
    std::string get_string(int index) override;
    
private:
    pqxx::result _raw;
    pqxx::result::const_iterator _iterator;
    bool _initialized = false;
};

} // namespace http

#if SERVER_WEB_HAVE_MYSQLCONNECTOR == 1

#include "mysql_result.h"

#include "Exceptions.h"

namespace http {

MysqlResult::MysqlResult(std::unique_ptr<sql::ResultSet>&& raw) noexcept
: _raw(std::move(raw)) {}

MysqlResult::MysqlResult(MysqlResult&& o) noexcept : _raw(std::move(o._raw)) {}

MysqlResult& MysqlResult::operator=(MysqlResult&& o) noexcept {
    _raw = std::move(o._raw);
    return *this;
}

bool MysqlResult::next() {
    return _raw && _raw->next();
}

int MysqlResult::get_int(int index) {
    if (!_raw) {
        throw NullException("MysqlResult::get_int: _raw is null");
    }
    return _raw->getInt(index);
}

float MysqlResult::get_float(int index) {
    if (!_raw) {
        throw NullException("MysqlResult::get_float: _raw is null");
    }
    return static_cast<float>(_raw->getDouble(index));
}

double MysqlResult::get_double(int index) {
    if (!_raw) {
        throw NullException("MysqlResult::get_double: _raw is null");
    }
    return _raw->getDouble(index);
}

std::string MysqlResult::get_string(int index) {
    if (!_raw) {
        throw NullException("MysqlResult::get_string: _raw is null");
    }
    return _raw->getString(index).asStdString();
}

} // namespace http

#endif

#include "postresql_result.h"

#include <stdexcept>

namespace http {

PqResult::PqResult(pqxx::result&& raw) noexcept : _raw(std::move(raw)) {}

PqResult::PqResult(PqResult&& o) noexcept : _raw(std::move(o._raw)) {}

PqResult& PqResult::operator=(PqResult&& o) noexcept {
    _raw = std::move(o._raw);
    _iterator = _raw.end();
    _initialized = false;
    return *this;
}

PqResult::operator bool() const {
    return !_raw.empty();
}

bool PqResult::next() {
    if (!_initialized) {
        _iterator = _raw.begin();
        _initialized = true;
        return _iterator != _raw.end();
    }
    return ++_iterator != _raw.end();
}

int PqResult::get_int(int index) {
    if (index <= 0) {
        throw std::out_of_range("PqResult::get_int: index must be >= 1");
    }
    return (*_iterator)[index - 1].as<int>();
}

float PqResult::get_float(int index) {
    if (index <= 0) {
        throw std::out_of_range("PqResult::get_float: index must be >= 1");
    }
    return (*_iterator)[index - 1].as<float>();
}

double PqResult::get_double(int index) {
    if (index <= 0) {
        throw std::out_of_range("PqResult::get_double: index must be >= 1");
    }
    return (*_iterator)[index - 1].as<double>();
}

std::string PqResult::get_string(int index) {
    if (index <= 0) {
        throw std::out_of_range("PqResult::get_string: index must be >= 1");
    }
    return (*_iterator)[index - 1].as<std::string>();
}

} // namespace http

#include "sql_result.h"

namespace http {

template <>
int SqlResult::as(int index) {
    return get_int(index);
}

template <>
float SqlResult::as(int index) {
    return get_float(index);
}

template <>
double SqlResult::as(int index) {
    return get_double(index);
}

template <>
std::string SqlResult::as(int index) {
    return get_string(index);
}

} // namespace http

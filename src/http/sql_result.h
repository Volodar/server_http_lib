#pragma once

#include <string>

namespace http {

class SqlResult {
public:
    SqlResult() = default;
    virtual ~SqlResult() = default;
    
    virtual bool next() = 0;
    virtual int get_int(int index) = 0;
    virtual float get_float(int index) = 0;
    virtual double get_double(int index) = 0;
    virtual std::string get_string(int index) = 0;
    
    template <class T>
    T as(int index);
};

} // namespace http

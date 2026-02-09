//
//  Exceptions.hpp
//  http
//
//  Created by Vladimir Tolmachev on 19.01.2026.
//

#ifndef Exceptions_hpp
#define Exceptions_hpp

#include <exception>
#include <string>

class NullException : public std::exception {
public:
    NullException(std::string&& msg) noexcept : _msg(std::move(msg)){}
    virtual const char* what() const noexcept{
        return _msg.c_str();
    }
private:
    std::string _msg;
    
};


class ResponseException : public std::exception {
public:
    ResponseException(int code, std::string&& body) noexcept : _code(code), _body(std::move(body)){
    }
    int get_code() const noexcept {return _code;}
    const std::string& get_body() const noexcept {return _body;}
private:
    int _code;
    std::string _body;
    
};

#endif /* Exceptions_hpp */

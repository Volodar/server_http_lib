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

class RuntimeException : public std::exception {
public:
    RuntimeException(std::string&& reason): _reason(std::move(reason)){}
    const char* what() const noexcept override {return _reason.data();}
private:
    std::string _reason;
};

class ResponseException : public std::exception {
public:
    ResponseException(int code, std::string&& body): _code(code), _body(std::move(body)){
    }
    const char* what() const noexcept override {return "ResponseException";}
    int get_code() const {return _code;}
    const std::string& get_body() const {return _body;}
private:
    int _code;
    std::string _body;
};

class ClientError : public ResponseException{
public:
    using ResponseException::ResponseException;
    const char* what() const noexcept override {return "ClientError";}
};

#endif /* Exceptions_hpp */

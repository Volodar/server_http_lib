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

class ResponseException : public std::exception {
public:
    ResponseException(int code, std::string&& body): _code(code), _body(std::move(body)){
    }
    int get_code() const {return _code;}
    const std::string& get_body() const {return _body;}
private:
    int _code;
    std::string _body;
    
};

#endif /* Exceptions_hpp */

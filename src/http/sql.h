//
//  sql.hpp
//  http
//
//  Created by Vladimir Tolmachev on 09.02.2026.
//
#pragma once

#include <string>
#include <cassert>

namespace http{

template <typename T> std::string to_sql_value(const T &value) {
    return std::to_string(value);
}
inline std::string to_sql_value(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() * 2);
    for(char c : value){
        if(c == '\''){
            escaped.push_back('\'');
            escaped.push_back('\'');
            continue;
        }
        if(c == '\\' || c == '"'){
            escaped.push_back('\\');
            escaped.push_back(c);
            continue;
        }
        escaped.push_back(c);
    }
    return escaped;
}
inline std::string to_sql_value(std::string_view value) {
    return to_sql_value(std::string(value));
}
inline std::string to_sql_value(const char *value) {
    return to_sql_value(std::string(value));
}
inline std::string to_sql_value(char *value) {
    return to_sql_value(std::string(value));
}

template <typename... Args>
std::string build_query(const std::string& template_str, Args &&...args) {
    std::string query = template_str;
    std::tuple<Args...> values(std::forward<Args>(args)...);
    
    constexpr int arg_count = sizeof...(Args);
    
    for (int i = arg_count - 1; i >= 0; --i) {
        std::string token = "$" + std::to_string(i);
        std::string value;
        
        std::apply(
                   [&](const auto &...unpacked) {
                       int index = 0;
                       ((index++ == i ? value = to_sql_value(unpacked)
                         : std::string()),
                        ...);
                   },
                   values);
        
        size_t pos = 0;
        bool was_replace = false;
        while ((pos = query.find(token, pos)) != std::string::npos) {
            query.replace(pos, token.length(), value);
            pos += value.length();
            was_replace = true;
        }
        (void)was_replace;
        assert(was_replace && "missing placeholder in query");
    }
    
    assert(query.find("$") == std::string::npos &&
           "unresolved placeholders remain");
    return query;
}

}

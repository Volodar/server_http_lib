//
//  Log.hpp
//  server_web
//
//  Created by Vladimir Tolmachev on 09.11.2025.
//

#ifndef Log_hpp
#define Log_hpp

#include <string>
#include <string_view>

#define log_error http::Log(http::Log::Level::error)
#define log_warning http::Log(http::Log::Level::warning)
#define log_info http::Log(http::Log::Level::info)
#define log_debug http::Log(http::Log::Level::debug)

namespace http{

class Log{
public:
    enum class Level{
        error,
        warning,
        info,
        debug,
    };
public:
    Log(Level level);
    ~Log();
    static void set_level(Level level);
    static Level get_level();
    Log& operator << (const std::string& message);
    Log& operator << (const std::string_view& message);
    Log& operator << (const char* message);
    Log& operator << (char* message);
    
    template <class T>
    Log& operator << (const T& data){
        if(_level > get_level()) return *this;
        append(std::to_string(data));
        return *this;
    }
protected:
    void append(const std::string& text);
private:
    Level _level;
    std::string _buffer;
};

}
#endif /* Log_hpp */

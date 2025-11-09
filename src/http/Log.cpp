//
//  Log.cpp
//  server_web
//
//  Created by Vladimir Tolmachev on 09.11.2025.
//

#include "Log.h"
#include <iostream>
#include "Scheduler.h"
#include <atomic>
#include <mutex>

namespace http{

static std::atomic<Log::Level> global_level = Log::Level::info;
static std::mutex log_mutex;

void Log::set_level(Level level){
    std::lock_guard<std::mutex> lock(log_mutex);
    global_level = level;
}
Log::Level Log::get_level(){
    std::lock_guard<std::mutex> lock(log_mutex);
    return global_level;
}

Log::Log(Level level): _level(level){
}
Log::~Log(){
    if(_level <= get_level()){
        std::lock_guard<std::mutex> lock(log_mutex);
        auto now = time(nullptr);
        std::string kind;
        if(_level == Level::info) kind = "[INFO]: ";
        if(_level == Level::warning) kind = "[WARNING]: ";
        if(_level == Level::error) kind = "[ERROR]: ";
        
        if(_level == Level::info)
            std::cout << "[" << timestamp_to_datetime(now) << "] " << kind <<_buffer << std::endl;
        else
            std::cerr << "[" << timestamp_to_datetime(now) << "] " << kind <<_buffer << std::endl;
    }
}

Log& Log::operator << (const std::string& message){
    append(message);
    return *this;
}
Log& Log::operator << (const std::string_view& message){
    if(_level > get_level()) return *this;
    append(std::string(message));
    return *this;
}
Log& Log::operator << (const char* message){
    if(_level > get_level()) return *this;
    append(message);
    return *this;
}
Log& Log::operator << (char* message){
    if(_level > get_level()) return *this;
    append(message);
    return *this;
}
void Log::append(const std::string& text){
    if(_level <= get_level()){
        _buffer += text;
    }    
}

}

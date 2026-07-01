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
#include <execinfo.h>
#include "utils.h"

namespace http{

std::string get_stack_trace() {
    std::string buffer = " -= STACKTRACE: =- \n================================================================================\n";
    buffer.reserve(1024*4);
    void* array[10];
    int size;
    size = backtrace(array, 10);
    char** symbols = backtrace_symbols(array, size);
    for (size_t i = 0; i < size; ++i) {
        buffer += symbols[i];
        buffer += "\n";
    }
    free(symbols); // Важно освободить память
    buffer += "================================================================================";
    return buffer;
}

static std::atomic<Log::Level> global_level = Log::Level::info;
static thread_local int worker_id = 0;

void Log::set_level(Level level){
    global_level = level;
}
Log::Level Log::level_from_str(std::string_view str){
    if(str == "error") return Level::error;
    if(str == "warning") return Level::warning;
    if(str == "info") return Level::info;
    if(str == "debug") return Level::debug;
    return Level::info;
}
Log::Level Log::get_level(){
    return global_level;
}

void Log::set_worker_id(int id){
    worker_id = id;
}

void Log::reset_worker_id(){
    worker_id = 0;
}

int Log::get_worker_id(){
    return worker_id;
}

std::mutex _out_mutex;

Log::Log(Level level): _level(level){
}
Log::~Log(){
    if(_level <= get_level()){
        auto now = time(nullptr);
        std::string kind;
        if(_level == Level::info)           kind =  "[INFO]: ";
        else if(_level == Level::warning)   kind =  "[WARN]: ";
        else if(_level == Level::error)     kind =  "[ERRR]: ";
        else if(_level == Level::debug)     kind =  "[DEBG]: ";
        
        auto id = get_worker_id();
        auto worker = id > 0 ? std::string("[WORKER:") + (id < 10 ? " " : "" ) + std::to_string(id) + "] " : "[  SERVER:] ";
        
        std::lock_guard<std::mutex> lock(_out_mutex);
        if(_level == Level::info)
            std::cout << "[" << timestamp_to_datetime(now) << "] " << worker << kind <<_buffer << std::endl;
        else
            std::cerr << "[" << timestamp_to_datetime(now) << "] " << worker << kind <<_buffer << std::endl;
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

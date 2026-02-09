#pragma once

#include "postresql_result.h"
#include "sql_wrapper.h"

#include "pqxx/connection"
#include "pqxx/result"
#include "pqxx/transaction"

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

namespace http {

class PostresqlWrapper : public SqlWrapperBase {
public:
    PostresqlWrapper();
    ~PostresqlWrapper();
    
    static bool test_connection(const std::string& host,
                                const std::string& login,
                                const std::string& password,
                                const std::string& dbname);
    
    void connect(const std::string& host, const std::string& login,
                 const std::string& password,
                 const std::string& dbname) override;
    void reconnect() override;
    void set_schema(const std::string& schema) override;
    
    bool query(const std::string& query) override;
    std::unique_ptr<SqlResult> query_get(const std::string& query) override;
    
    std::shared_ptr<pqxx::connection> get_connection();
    
protected:
    void release_connection(pqxx::connection* conn);
    
private:
    std::string _host;
    std::string _user;
    std::string _password;
    std::string _schema;
    std::deque<pqxx::connection*> _connections;
    std::mutex _mutex;
    std::condition_variable _condition;
};

} // namespace http

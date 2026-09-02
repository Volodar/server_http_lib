#include "test_framework.h"
#include "http/mysql_wrapper.h"
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#if SERVER_WEB_HAVE_MYSQLCONNECTOR == 1

TEST(mysql_wrapper_integration_replaces_connection_closed_by_server) {
    auto host = std::getenv("MYSQL_HOST");
    auto user = std::getenv("MYSQL_USER");
    auto password = std::getenv("MYSQL_PASSWORD");
    auto database = std::getenv("MYSQL_DATABASE");
    if (!host || !user || !password || !database) {
        std::cout << "[ INFO ] MySQL integration test skipped: MYSQL_HOST, MYSQL_USER, MYSQL_PASSWORD and MYSQL_DATABASE are required\n";
        return;
    }

    auto mysql = std::make_shared<MysqlWrapper>();
    mysql->connect(host, user, password);
    mysql->set_schema(database);

    auto stale_connection = mysql->get_connection();
    unsigned long long connection_id = 0;
    {
        std::unique_ptr<sql::Statement> statement(stale_connection->createStatement());
        std::unique_ptr<sql::ResultSet> result(statement->executeQuery("SELECT CONNECTION_ID()"));
        ASSERT_TRUE(result->next());
        connection_id = result->getUInt64(1);
    }

    auto killer_connection = mysql->get_connection();
    {
        std::unique_ptr<sql::Statement> statement(killer_connection->createStatement());
        statement->execute("KILL CONNECTION " + std::to_string(connection_id));
    }
    killer_connection.reset();
    stale_connection.reset();

    auto connection = mysql->get_connection();
    ASSERT_FALSE(connection->isClosed());
    ASSERT_TRUE(connection->isValid());

    std::unique_ptr<sql::Statement> statement(connection->createStatement());
    std::unique_ptr<sql::ResultSet> result(statement->executeQuery("SELECT DATABASE(), 1"));
    ASSERT_TRUE(result->next());
    ASSERT_EQ(result->getString(1).asStdString(), connection->getSchema().asStdString());
    ASSERT_EQ(result->getInt(2), 1);
}

#endif

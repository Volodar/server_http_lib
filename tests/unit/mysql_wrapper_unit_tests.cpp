#include "test_framework.h"
#include "http/mysql_wrapper.h"

#if SERVER_WEB_HAVE_MYSQLCONNECTOR == 1

TEST(mysql_wrapper_unit_keeps_open_valid_connection) {
    ASSERT_FALSE(MysqlWrapper::should_replace_connection(false, true));
}

TEST(mysql_wrapper_unit_replaces_closed_connection) {
    ASSERT_TRUE(MysqlWrapper::should_replace_connection(true, true));
}

TEST(mysql_wrapper_unit_replaces_invalid_connection) {
    ASSERT_TRUE(MysqlWrapper::should_replace_connection(false, false));
}

#endif

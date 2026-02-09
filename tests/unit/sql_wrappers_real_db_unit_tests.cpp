#include "test_framework.h"

#if SERVER_WEB_HAVE_MYSQLCONNECTOR == 1

#include "http/mysql_wrapper.h"
#include "http/postresql_wrapper.h"

#include <chrono>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr const char* kTestSchema = "server_http_tests";

struct DbRuntimeConfig {
  std::string host;
  std::string user;
  std::string password;
  std::string dbname;
};

template <class TWrapper>
struct RealDbTraits;

template <>
struct RealDbTraits<http::MysqlWrapper> {
  static bool load(DbRuntimeConfig& cfg) {
    const char* host = std::getenv("TEST_MYSQL_HOST");
    const char* user = std::getenv("TEST_MYSQL_USER");
    const char* password = std::getenv("TEST_MYSQL_PASSWORD");
    if (!host || !user || !password) {
      return false;
    }
    cfg.host = host;
    cfg.user = user;
    cfg.password = password;
    cfg.dbname = kTestSchema;
    return true;
  }
};

template <>
struct RealDbTraits<http::PostresqlWrapper> {
  static bool load(DbRuntimeConfig& cfg) {
    const char* host = std::getenv("TEST_POSTGRES_HOST");
    const char* user = std::getenv("TEST_POSTGRES_USER");
    const char* password = std::getenv("TEST_POSTGRES_PASSWORD");
    const char* dbname = kTestSchema;
    if (!host || !user || !password || !dbname) {
      return false;
    }
    cfg.host = host;
    cfg.user = user;
    cfg.password = password;
    cfg.dbname = dbname;
    return true;
  }
};

template <class TWrapper>
void connect_wrapper(TWrapper& wrapper, const DbRuntimeConfig& cfg) {
  wrapper.connect(cfg.host, cfg.user, cfg.password, cfg.dbname);
}

template <class TWrapper>
bool ensure_test_schema(TWrapper& wrapper) {
  return wrapper.query(std::string("CREATE SCHEMA IF NOT EXISTS ") + kTestSchema);
}

template <class TWrapper>
std::string make_table_name() {
  auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  std::ostringstream oss;
  oss << "sql_wrapper_real_" << now;
  return oss.str();
}

template <class TWrapper>
bool ready_for_real_db() {
  DbRuntimeConfig cfg;
  if (!RealDbTraits<TWrapper>::load(cfg)) {
    return false;
  }

  ::setenv("MYSQL_CONN", "1", 1);

  TWrapper wrapper;
  connect_wrapper(wrapper, cfg);
  if (!ensure_test_schema(wrapper)) {
    return false;
  }
  return wrapper.query("SELECT 1");
}

template <class TWrapper>
void test_real_query_roundtrip() {
  DbRuntimeConfig cfg;
  if (!RealDbTraits<TWrapper>::load(cfg)) {
    return;
  }

  ::setenv("MYSQL_CONN", "1", 1);

  TWrapper wrapper;
  connect_wrapper(wrapper, cfg);

  ASSERT_TRUE(ensure_test_schema(wrapper));

  const std::string table = make_table_name<TWrapper>();
  const std::string fq_table = std::string(kTestSchema) + "." + table;

  ASSERT_TRUE(wrapper.query("DROP TABLE IF EXISTS " + fq_table));
  ASSERT_TRUE(wrapper.query("CREATE TABLE " + fq_table +
                            " (id INT PRIMARY KEY, value_text VARCHAR(64))"));
  ASSERT_TRUE(wrapper.query("INSERT INTO " + fq_table +
                            " (id, value_text) VALUES (1, 'one'), (2, 'two')"));

  auto result =
      wrapper.query_get("SELECT id, value_text FROM " + fq_table + " ORDER BY id");

  std::vector<int> ids;
  std::vector<std::string> values;
  while (result->next()) {
    ids.push_back(result->get_int(1));
    values.push_back(result->get_string(2));
  }

  ASSERT_EQ(ids.size(), 2u);
  ASSERT_EQ(ids[0], 1);
  ASSERT_EQ(ids[1], 2);
  ASSERT_EQ(values[0], std::string("one"));
  ASSERT_EQ(values[1], std::string("two"));

  ASSERT_TRUE(wrapper.query("DROP TABLE IF EXISTS " + fq_table));
}

template <class TWrapper>
void test_real_reconnect_keeps_operable() {
  DbRuntimeConfig cfg;
  if (!RealDbTraits<TWrapper>::load(cfg)) {
    return;
  }

  ::setenv("MYSQL_CONN", "1", 1);

  TWrapper wrapper;
  connect_wrapper(wrapper, cfg);

  ASSERT_TRUE(ensure_test_schema(wrapper));
  ASSERT_TRUE(wrapper.query("SELECT 1"));

  wrapper.reconnect();
  ASSERT_TRUE(wrapper.query("SELECT 1"));
}

}  // namespace

TEST(SQL_WRAPPER_REAL_DB_QueryRoundtrip) {
    const bool mysql_ready = ready_for_real_db<http::MysqlWrapper>();
    const bool postgres_ready = ready_for_real_db<http::PostresqlWrapper>();
    ASSERT_TRUE (mysql_ready);
    ASSERT_TRUE(postgres_ready);
    
    if (mysql_ready) {
        test_real_query_roundtrip<http::MysqlWrapper>();
    }
    if (postgres_ready) {
        test_real_query_roundtrip<http::PostresqlWrapper>();
    }
}

TEST(SQL_WRAPPER_REAL_DB_Reconnect) {
    const bool mysql_ready = ready_for_real_db<http::MysqlWrapper>();
    const bool postgres_ready = ready_for_real_db<http::PostresqlWrapper>();
    ASSERT_TRUE (mysql_ready);
    ASSERT_TRUE(postgres_ready);
    
    if (mysql_ready) {
        test_real_reconnect_keeps_operable<http::MysqlWrapper>();
    }
    if (postgres_ready) {
        test_real_reconnect_keeps_operable<http::PostresqlWrapper>();
    }
}

#endif

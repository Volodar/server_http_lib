#include "test_framework.h"

#if SERVER_WEB_HAVE_MYSQLCONNECTOR == 1

#include "http/mysql_wrapper.h"
#include "http/postresql_wrapper.h"

#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

class FakeSqlResult : public http::SqlResult {
public:
  explicit FakeSqlResult(std::vector<std::vector<std::string>> rows)
      : _rows(std::move(rows)) {}

  bool next() override {
    if (_rows.empty()) {
      return false;
    }
    if (!_started) {
      _started = true;
      _index = 0;
      return true;
    }
    ++_index;
    return _index < _rows.size();
  }

  int get_int(int index) override { return std::stoi(get_string(index)); }

  float get_float(int index) override { return std::stof(get_string(index)); }

  double get_double(int index) override { return std::stod(get_string(index)); }

  std::string get_string(int index) override {
    if (!_started || _index >= _rows.size() || index <= 0 ||
        static_cast<size_t>(index) > _rows[_index].size()) {
      return "";
    }
    return _rows[_index][static_cast<size_t>(index - 1)];
  }

private:
  std::vector<std::vector<std::string>> _rows;
  size_t _index = 0;
  bool _started = false;
};

template <class TWrapper>
class WrapperSpy : public TWrapper {
public:
  std::vector<std::string> executed_queries;
  std::vector<std::string> query_get_calls;
  std::deque<std::unique_ptr<http::SqlResult>> scripted_results;

  void connect(const std::string& host, const std::string& login,
               const std::string& password,
               const std::string& schema) override {
    (void)host;
    (void)login;
    (void)password;
    (void)schema;
  }

  void reconnect() override {}

  void set_schema(const std::string& schema) override { (void)schema; }

  bool query(const std::string& query) override {
    executed_queries.push_back(query);
    return true;
  }

  std::unique_ptr<http::SqlResult> query_get(const std::string& query) override {
    query_get_calls.push_back(query);
    if (scripted_results.empty()) {
      return std::make_unique<FakeSqlResult>(
          std::vector<std::vector<std::string>>{});
    }
    auto out = std::move(scripted_results.front());
    scripted_results.pop_front();
    return out;
  }
};

template <class TWrapper>
std::unique_ptr<http::SqlResult> make_rows(
    std::vector<std::vector<std::string>> rows) {
  return std::make_unique<FakeSqlResult>(std::move(rows));
}

template <class TWrapper>
void test_has_table_true() {
  WrapperSpy<TWrapper> wrapper;
  wrapper.scripted_results.push_back(make_rows<TWrapper>({{"users"}}));

  ASSERT_TRUE(wrapper.has_table("main", "users"));
  ASSERT_EQ(wrapper.query_get_calls.size(), 1u);
  ASSERT_EQ(wrapper.query_get_calls[0], std::string("SHOW TABLES FROM main"));
}

template <class TWrapper>
void test_create_table_skip_when_exists() {
  WrapperSpy<TWrapper> wrapper;
  wrapper.scripted_results.push_back(make_rows<TWrapper>({{"users"}}));

  wrapper.create_table("main", "users", "CREATE TABLE $0.$1 (id INT);");

  ASSERT_EQ(wrapper.executed_queries.size(), 0u);
}

template <class TWrapper>
void test_create_table_executes_split_queries() {
  WrapperSpy<TWrapper> wrapper;
  wrapper.scripted_results.push_back(
      make_rows<TWrapper>(std::vector<std::vector<std::string>>{}));

  wrapper.create_table("main", "users",
                       " CREATE TABLE $0.$1 (id INT); ;"
                       " ALTER TABLE $0.$1 ADD COLUMN name TEXT; ");

  ASSERT_EQ(wrapper.executed_queries.size(), 2u);
  ASSERT_EQ(wrapper.executed_queries[0],
            std::string("CREATE TABLE main.users (id INT)"));
  ASSERT_EQ(wrapper.executed_queries[1],
            std::string("ALTER TABLE main.users ADD COLUMN name TEXT"));
}

template <class TWrapper>
void test_alter_table_add_column_throws_if_table_missing() {
  WrapperSpy<TWrapper> wrapper;
  wrapper.scripted_results.push_back(
      make_rows<TWrapper>(std::vector<std::vector<std::string>>{}));

  bool thrown = false;
  try {
    wrapper.alter_table_add_column("main", "users", "age", "INT DEFAULT 0");
  } catch (const sql::SQLException&) {
    thrown = true;
  }
  ASSERT_TRUE(thrown);
}

template <class TWrapper>
void test_alter_table_add_column_skip_if_exists() {
  WrapperSpy<TWrapper> wrapper;
  wrapper.scripted_results.push_back(make_rows<TWrapper>({{"users"}}));
  wrapper.scripted_results.push_back(make_rows<TWrapper>({{"1"}}));

  wrapper.alter_table_add_column("main", "users", "age", "INT DEFAULT 0");

  ASSERT_EQ(wrapper.executed_queries.size(), 0u);
}

template <class TWrapper>
void test_alter_table_add_column_executes_when_missing() {
  WrapperSpy<TWrapper> wrapper;
  wrapper.scripted_results.push_back(make_rows<TWrapper>({{"users"}}));
  wrapper.scripted_results.push_back(make_rows<TWrapper>({{"0"}}));

  wrapper.alter_table_add_column("main", "users", "age", "INT DEFAULT 0");

  ASSERT_EQ(wrapper.executed_queries.size(), 1u);
  ASSERT_EQ(wrapper.executed_queries[0],
            std::string("ALTER TABLE main.users ADD COLUMN age INT DEFAULT 0;"));
}

template <class TWrapper>
void test_alter_table_change_column_skip_if_same_type() {
  WrapperSpy<TWrapper> wrapper;
  wrapper.scripted_results.push_back(make_rows<TWrapper>({{"age", "int"}}));

  wrapper.alter_table_change_column("main", "users", "age", "int");

  ASSERT_EQ(wrapper.executed_queries.size(), 0u);
}

template <class TWrapper>
void test_alter_table_change_column_executes_if_different_type() {
  WrapperSpy<TWrapper> wrapper;
  wrapper.scripted_results.push_back(make_rows<TWrapper>({{"age", "bigint"}}));

  wrapper.alter_table_change_column("main", "users", "age", "int");

  ASSERT_EQ(wrapper.executed_queries.size(), 1u);
  ASSERT_EQ(wrapper.executed_queries[0],
            std::string("ALTER TABLE main.users MODIFY COLUMN age int"));
}

template <class TWrapper>
void test_create_index_throws_if_table_missing() {
  WrapperSpy<TWrapper> wrapper;
  wrapper.scripted_results.push_back(
      make_rows<TWrapper>(std::vector<std::vector<std::string>>{}));

  bool thrown = false;
  try {
    wrapper.create_index("main", "users", "email", "");
  } catch (const sql::SQLException&) {
    thrown = true;
  }
  ASSERT_TRUE(thrown);
}

template <class TWrapper>
void test_create_index_skip_if_exists() {
  WrapperSpy<TWrapper> wrapper;
  wrapper.scripted_results.push_back(make_rows<TWrapper>({{"users"}}));
  wrapper.scripted_results.push_back(
      make_rows<TWrapper>({{"", "", "", "", "email"}}));

  wrapper.create_index("main", "users", "email", "");

  ASSERT_EQ(wrapper.executed_queries.size(), 0u);
}

template <class TWrapper>
void test_create_index_executes_default_query() {
  WrapperSpy<TWrapper> wrapper;
  wrapper.scripted_results.push_back(make_rows<TWrapper>({{"users"}}));
  wrapper.scripted_results.push_back(
      make_rows<TWrapper>(std::vector<std::vector<std::string>>{}));

  wrapper.create_index("main", "users", "email", "");

  ASSERT_EQ(wrapper.executed_queries.size(), 1u);
  ASSERT_EQ(wrapper.executed_queries[0],
            std::string("CREATE INDEX idx_email ON main.users (email);"));
}

template <class TWrapper>
void test_create_index_executes_custom_query() {
  WrapperSpy<TWrapper> wrapper;
  wrapper.scripted_results.push_back(make_rows<TWrapper>({{"users"}}));
  wrapper.scripted_results.push_back(
      make_rows<TWrapper>(std::vector<std::vector<std::string>>{}));

  wrapper.create_index("main", "users", "email",
                       "CREATE INDEX custom_idx ON users(email)");

  ASSERT_EQ(wrapper.executed_queries.size(), 1u);
  ASSERT_EQ(wrapper.executed_queries[0],
            std::string("CREATE INDEX custom_idx ON users(email)"));
}

} // namespace

TEST(SQL_WRAPPER_HasTable_True) {
  test_has_table_true<http::MysqlWrapper>();
  test_has_table_true<http::PostresqlWrapper>();
}

TEST(SQL_WRAPPER_CreateTable_SkipWhenExists) {
  test_create_table_skip_when_exists<http::MysqlWrapper>();
  test_create_table_skip_when_exists<http::PostresqlWrapper>();
}

TEST(SQL_WRAPPER_CreateTable_ExecutesSplitQueries) {
  test_create_table_executes_split_queries<http::MysqlWrapper>();
  test_create_table_executes_split_queries<http::PostresqlWrapper>();
}

TEST(SQL_WRAPPER_AlterAddColumn_ThrowsIfTableMissing) {
  test_alter_table_add_column_throws_if_table_missing<http::MysqlWrapper>();
  test_alter_table_add_column_throws_if_table_missing<http::PostresqlWrapper>();
}

TEST(SQL_WRAPPER_AlterAddColumn_SkipIfExists) {
  test_alter_table_add_column_skip_if_exists<http::MysqlWrapper>();
  test_alter_table_add_column_skip_if_exists<http::PostresqlWrapper>();
}

TEST(SQL_WRAPPER_AlterAddColumn_ExecutesWhenMissing) {
  test_alter_table_add_column_executes_when_missing<http::MysqlWrapper>();
  test_alter_table_add_column_executes_when_missing<http::PostresqlWrapper>();
}

TEST(SQL_WRAPPER_AlterChangeColumn_SkipIfSameType) {
  test_alter_table_change_column_skip_if_same_type<http::MysqlWrapper>();
  test_alter_table_change_column_skip_if_same_type<http::PostresqlWrapper>();
}

TEST(SQL_WRAPPER_AlterChangeColumn_ExecutesIfDifferentType) {
  test_alter_table_change_column_executes_if_different_type<http::MysqlWrapper>();
  test_alter_table_change_column_executes_if_different_type<http::PostresqlWrapper>();
}

TEST(SQL_WRAPPER_CreateIndex_ThrowsIfTableMissing) {
  test_create_index_throws_if_table_missing<http::MysqlWrapper>();
  test_create_index_throws_if_table_missing<http::PostresqlWrapper>();
}

TEST(SQL_WRAPPER_CreateIndex_SkipIfExists) {
  test_create_index_skip_if_exists<http::MysqlWrapper>();
  test_create_index_skip_if_exists<http::PostresqlWrapper>();
}

TEST(SQL_WRAPPER_CreateIndex_ExecutesDefaultQuery) {
  test_create_index_executes_default_query<http::MysqlWrapper>();
  test_create_index_executes_default_query<http::PostresqlWrapper>();
}

TEST(SQL_WRAPPER_CreateIndex_ExecutesCustomQuery) {
  test_create_index_executes_custom_query<http::MysqlWrapper>();
  test_create_index_executes_custom_query<http::PostresqlWrapper>();
}

#endif

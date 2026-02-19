#include "test_framework.h"
#include "http/utils.h"
#include "http/mysql_wrapper.h"
#include <string>
#include <vector>
#include <fstream>
#include <cstdio>

TEST(Utils_Split)
{
    using http::split;
    // Empty string -> empty result
    auto v0 = split("", ',');
    ASSERT_EQ(v0.size(), 0u);

    // Basic split
    auto v1 = split("a,b,c", ',');
    ASSERT_EQ(v1.size(), 3u);
    ASSERT_EQ(v1[0], std::string("a"));
    ASSERT_EQ(v1[1], std::string("b"));
    ASSERT_EQ(v1[2], std::string("c"));

    // Consecutive delimiters preserve empty tokens
    auto v2 = split("a,,b", ',');
    ASSERT_EQ(v2.size(), 3u);
    ASSERT_EQ(v2[0], std::string("a"));
    ASSERT_EQ(v2[1], std::string(""));
    ASSERT_EQ(v2[2], std::string("b"));

    // Trailing delimiter is ignored
    auto v3 = split("trailing,", ',');
    ASSERT_EQ(v3.size(), 1u);
    ASSERT_EQ(v3[0], std::string("trailing"));
}

TEST(Utils_Strip)
{
    // Trim only leading and trailing whitespace, preserve internal spaces
    std::string s1 = "\t  hello  world  \t";
    http::strip(s1);
    ASSERT_EQ(s1, std::string("hello  world"));

    // Empty stays empty
    std::string s2;
    http::strip(s2);
    ASSERT_EQ(s2, std::string(""));

    // All whitespace becomes empty
    std::string s3 = " \t  \t ";
    http::strip(s3);
    ASSERT_EQ(s3, std::string(""));
}

TEST(Utils_StringView_Strip)
{
    std::string s = "\t hello\t ";

    std::string_view sv1 = s;
    http::sv_lstrip(sv1);
    ASSERT_EQ(std::string(sv1), std::string("hello\t "));

    std::string_view sv2 = s;
    http::sv_rstrip(sv2);
    ASSERT_EQ(std::string(sv2), std::string("\t hello"));

    std::string_view sv3 = s;
    http::sv_strip(sv3);
    ASSERT_EQ(std::string(sv3), std::string("hello"));
}

TEST(Utils_Replace)
{
    std::string a = "aabb";
    http::replace(a, "aa", "x");
    ASSERT_EQ(a, std::string("xbb"));
    http::replace(a, "b", "yy");
    ASSERT_EQ(a, std::string("xyyyy"));

    // Empty 'from' is a no-op
    std::string b = "abc";
    http::replace(b, "", "Z");
    ASSERT_EQ(b, std::string("abc"));

    // Non-overlapping sequential replacements
    std::string c = "aaaa";
    http::replace(c, "aa", "b");
    ASSERT_EQ(c, std::string("bb"));
}

TEST(Utils_GetFileExt)
{
    ASSERT_EQ(std::string(http::get_file_ext("index.html")), std::string(".html"));
    ASSERT_EQ(std::string(http::get_file_ext("archive.tar.gz")), std::string(".gz"));
    ASSERT_EQ(std::string(http::get_file_ext(".gitignore")), std::string(".gitignore"));
}

TEST(Utils_GetContentType)
{
    ASSERT_EQ(http::get_content_type("index.html"), std::string(http::ContentType::Html));
    ASSERT_EQ(http::get_content_type("styles.css"), std::string(http::ContentType::Css));
    ASSERT_EQ(http::get_content_type("script.js"), std::string(http::ContentType::Js));
    ASSERT_EQ(http::get_content_type("script.json"), std::string(http::ContentType::Json));
    ASSERT_EQ(http::get_content_type("image.png"), std::string(http::ContentType::Png));
    ASSERT_EQ(http::get_content_type("file.unknown"), std::string(http::ContentType::OctetStream));
}

TEST(Utils_GetFileContent)
{
    const char* path = "tmp_utils_test.txt";
    {
        std::ofstream out(path, std::ios::binary);
        out << "hello\nworld";
    }
    auto content = http::get_file_content(path);
    ASSERT_EQ(content, std::string("hello\nworld"));
    std::remove(path);
}

#if SERVER_WEB_HAVE_MYSQLCONNECTOR == 1
TEST(Utils_BuildQuery_EscapesInsideQuotedLiteral)
{
    auto query = build_query("SELECT * FROM users WHERE login='$0'", std::string("o'reilly"));
    ASSERT_EQ(query, "SELECT * FROM users WHERE login='o''reilly'");
}

TEST(Utils_BuildQuery_AllowsRawSqlFragmentOutsideQuotedLiteral)
{
    auto filter = std::string("WHERE COALESCE(c.club_type, 'open')='open'");
    auto query = build_query("SELECT c.id FROM clubs c $0 ORDER BY c.id DESC", filter);
    ASSERT_EQ(query, "SELECT c.id FROM clubs c WHERE COALESCE(c.club_type, 'open')='open' ORDER BY c.id DESC");
}

TEST(Utils_BuildQuery_MixedQuotedAndRaw)
{
    auto filter = build_query("WHERE title LIKE '%%$0%%'", std::string("t"));
    auto query = build_query("SELECT id FROM clubs $0", filter);
    ASSERT_EQ(query, "SELECT id FROM clubs WHERE title LIKE '%%t%%'");
}
TEST(Utils_BuildQuery_EmptyStringValue)
{
    auto query = build_query(R"(SELECT * FROM users WHERE login='$0' AND user="")", "");
    ASSERT_EQ(query, R"(SELECT * FROM users WHERE login='' AND user="")");

    query = build_query(R"(SELECT * FROM users WHERE login="$0" AND user='')", "");
    ASSERT_EQ(query, R"(SELECT * FROM users WHERE login="" AND user='')");
}
#endif

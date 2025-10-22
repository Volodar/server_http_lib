#include "test_framework.h"
#include "http/utils.h"
#include <string>

TEST(UrlEncode_Simple)
{
    ASSERT_EQ(http::url_encode("abcXYZ-_.~"), std::string("abcXYZ-_.~"));
    ASSERT_EQ(http::url_encode("hello world"), std::string("hello+world"));
    ASSERT_EQ(http::url_encode("a+b"), std::string("a%2Bb"));
    ASSERT_EQ(http::url_encode("100%"), std::string("100%25"));
}

TEST(UrlDecode_Simple)
{
    ASSERT_EQ(http::url_decode("abcXYZ-_.~"), std::string("abcXYZ-_.~"));
    ASSERT_EQ(http::url_decode("hello+world"), std::string("hello world"));
    ASSERT_EQ(http::url_decode("a%2Bb"), std::string("a+b"));
    ASSERT_EQ(http::url_decode("100%25"), std::string("100%"));
}

TEST(UrlCodec_RoundTrip)
{
    std::string original = " !#?$&='()*,/:;@[]{}|\\^`~<>\"";
    auto enc = http::url_encode(original);
    auto dec = http::url_decode(enc);
    ASSERT_EQ(dec, original);
}


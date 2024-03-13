#include <gtest/gtest.h>
#include <url.h>

TEST(UrlTest, Parse) {
  {
    auto url = Url::parse("http://www.google.com");
    EXPECT_EQ(UrlScheme::SCM_HTTP, url.scheme);
  }
  {
    auto url =
        Url::parse("https://datatracker.ietf.org/doc/html/rfc3986#section-3.1");
    EXPECT_EQ(UrlScheme::SCM_HTTP, url.scheme);
  }
}

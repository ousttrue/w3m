#include <gtest/gtest.h>
#include <url.h>

TEST(UrlTest, Parse) {
  {
    auto url = Url::parse("http://www.google.com");
    EXPECT_EQ(UrlScheme::SCM_HTTP, url.scheme);
    EXPECT_EQ("www.google.com", url.host);
  }
  {
    auto url =
        Url::parse("https://datatracker.ietf.org/doc/html/rfc3986#section-3.1");
    EXPECT_EQ(UrlScheme::SCM_HTTPS, url.scheme);
    EXPECT_EQ("datatracker.ietf.org", url.host);
    EXPECT_EQ("/doc/html/rfc3986", url.file);
    EXPECT_EQ("section-3.1", url.label);
  }
}

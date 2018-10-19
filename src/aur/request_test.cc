#include "request.hh"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

constexpr char kBaseUrl[] = "http://aur.archlinux.org";

using testing::AllOf;
using testing::EndsWith;
using testing::HasSubstr;
using testing::StartsWith;

TEST(RequestTest, BuildsInfoRequests) {
  aur::InfoRequest request;

  request.AddArg("derp");

  const auto urls = request.Build(kBaseUrl);
  ASSERT_EQ(urls.size(), 1);

  EXPECT_THAT(urls[0], AllOf(StartsWith(kBaseUrl), HasSubstr("v=5"),
                             HasSubstr("type=info"), HasSubstr("arg[]=derp")));
}

TEST(RequestTest, UrlEncodesParameterValues) {
  aur::InfoRequest request;

  request.AddArg("c++");

  const auto urls = request.Build(kBaseUrl);
  EXPECT_THAT(urls[0], HasSubstr("arg[]=c%2B%2B"));
}

TEST(RequestTest, BuildsMultipleUrlsForLongRequests) {
  aur::RpcRequest request({{"v", "5"}}, 100);

  const std::string longarg(14, 'a');
  for (int i = 0; i < 10; ++i) {
    request.AddArg("arg[]", longarg);
  }

  // Builds more than one URL because we go over the limit.
  const auto urls = request.Build(kBaseUrl);
  ASSERT_EQ(urls.size(), 3);

  const auto arg = "&arg[]=" + longarg;
  for (const auto& url : urls) {
    // URLs aren't truncated
    EXPECT_THAT(url, EndsWith(arg));
    // We've trimmed things correctly in chopping up the querystring
    EXPECT_THAT(url, testing::Not(testing::HasSubstr("&&")));
  }
}

TEST(RequestTest, BuildsSearchRequests) {
  aur::SearchRequest request(aur::SearchRequest::SearchBy::MAINTAINER);

  request.AddArg("bar");
  request.AddArg("foo");

  const auto urls = request.Build(kBaseUrl);
  ASSERT_EQ(urls.size(), 1);

  EXPECT_THAT(urls[0], AllOf(HasSubstr("v=5"), HasSubstr("by=maintainer"),
                             HasSubstr("type=search"), HasSubstr("arg=foo")));
}

TEST(RequestTest, BuildsRawRequests) {
  aur::RawRequest request("/foo/bar/baz");

  const auto urls = request.Build(kBaseUrl);
  ASSERT_EQ(urls.size(), 1);

  EXPECT_EQ(urls[0], std::string(kBaseUrl) + "/foo/bar/baz");
}

TEST(RequestTest, BuildsCloneRequests) {
  const std::string kReponame = "auracle-git";

  aur::CloneRequest request(kReponame);

  ASSERT_EQ(request.reponame(), kReponame);

  const auto urls = request.Build(kBaseUrl);
  ASSERT_EQ(urls.size(), 1);

  const auto& url = urls[0];
  EXPECT_EQ(url, std::string(kBaseUrl) + "/" + kReponame);
}

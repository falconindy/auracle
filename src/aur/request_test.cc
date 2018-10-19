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
  aur::InfoRequest request;

  const std::string longarg(22, 'a');
  for (int i = 0; i < 250; ++i) {
    request.AddArg(longarg);
  }

  // Builds more than one URL because we go over the limit.
  const auto urls = request.Build(kBaseUrl);
  ASSERT_EQ(urls.size(), 2);

  // URLs aren't truncated
  const auto arg = "&arg[]=" + longarg;
  EXPECT_THAT(urls[0], EndsWith(arg));
  EXPECT_THAT(urls[1], EndsWith(arg));
}

TEST(RequestTest, BuildsSearchRequests) {
  aur::SearchRequest request;

  request.AddArg("bar");
  request.AddArg("foo");

  request.SetSearchBy(aur::SearchRequest::SearchBy::MAINTAINER);

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

// SPDX-License-Identifier: MIT
#include "aur/request.hh"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

constexpr char kBaseUrl[] = "http://aur.archlinux.org";

using testing::AllOf;
using testing::EndsWith;
using testing::HasSubstr;
using testing::Not;
using testing::StartsWith;

TEST(RequestTest, BuildsInfoRequests) {
  aur::InfoRequest request;

  request.AddArg("derp");

  const auto url = request.Url(kBaseUrl);
  EXPECT_THAT(url, AllOf(EndsWith("/rpc/v5/info")));

  const auto payload = request.Payload();
  EXPECT_EQ(payload, "arg[]=derp");
}

TEST(RequestTest, UrlEncodesParameterValues) {
  aur::InfoRequest irequest;

  irequest.AddArg("c++");

  const auto payload = irequest.Payload();
  EXPECT_EQ(payload, "arg[]=c%2B%2B");

  aur::SearchRequest srequest(aur::SearchRequest::SearchBy::NAME_DESC, "A B");
  const auto url = srequest.Url("http://a.b");
  EXPECT_THAT(url, StartsWith("http://a.b/rpc/v5/search/A%20B?"));
}

TEST(RequestTest, BuildsSearchRequests) {
  aur::SearchRequest request(aur::SearchRequest::SearchBy::MAINTAINER, "foo");

  const std::string url = request.Url(kBaseUrl);

  EXPECT_THAT(url,
              AllOf(testing::EndsWith("/rpc/v5/search/foo?by=maintainer")));
}

TEST(RequestTest, BuildsRawRequests) {
  aur::RawRequest request("/foo/bar/baz");

  const auto url = request.Url(kBaseUrl);

  EXPECT_EQ(url, std::string(kBaseUrl) + "/foo/bar/baz");
}

TEST(RequestTest, UrlForSourceFileEscapesReponame) {
  aur::Package p;
  p.pkgbase = "libc++";
  auto request = aur::RawRequest::ForSourceFile(p, "PKGBUILD");

  auto url = request.Url(kBaseUrl);

  EXPECT_THAT(url, EndsWith("/PKGBUILD?h=libc%2B%2B"));
}

TEST(RequestTest, UrlForSourceFileEscapesFilename) {
  aur::Package p;
  p.pkgbase = "auracle-git";

  EXPECT_THAT(
      aur::RawRequest::ForSourceFile(p, "foo bar.install").Url(kBaseUrl),
      EndsWith("/foo%20bar.install?h=auracle-git"));

  EXPECT_THAT(aur::RawRequest::ForSourceFile(p, "foo#bar").Url(kBaseUrl),
              EndsWith("/foo%23bar?h=auracle-git"));

  EXPECT_THAT(aur::RawRequest::ForSourceFile(p, "foo?h=evil").Url(kBaseUrl),
              EndsWith("/foo%3Fh%3Devil?h=auracle-git"));
}

TEST(RequestTest, UrlForSourceFilePreservesPathSeparators) {
  aur::Package p;
  p.pkgbase = "auracle-git";

  EXPECT_THAT(
      aur::RawRequest::ForSourceFile(p, "keys/pgp/foo.asc").Url(kBaseUrl),
      EndsWith("/keys/pgp/foo.asc?h=auracle-git"));
}

TEST(RequestTest, BuildsCloneRequests) {
  const std::string kReponame = "auracle-git";

  aur::CloneRequest request(kReponame);

  ASSERT_EQ(request.reponame(), kReponame);

  const auto url = request.Url(kBaseUrl);

  EXPECT_EQ(url, std::string(kBaseUrl) + "/" + kReponame);
}

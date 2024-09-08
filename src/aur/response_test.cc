// SPDX-License-Identifier: MIT
#include "aur/response.hh"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using aur::RpcResponse;
using testing::Field;
using testing::UnorderedElementsAre;

TEST(ResponseTest, ParsesSuccessResponse) {
  const auto response = RpcResponse::Parse(R"({
    "version": 5,
    "type": "multiinfo",
    "resultcount": 1,
    "results": [
      {
        "ID": 534056,
        "Name": "auracle-git",
        "PackageBaseID": 123768,
        "PackageBase": "auracle-git",
        "Version": "r36.752e4ba-1",
        "Description": "A flexible client for the AUR",
        "URL": "https://github.com/falconindy/auracle.git",
        "NumVotes": 15,
        "Popularity": 0.095498,
        "OutOfDate": null,
        "Maintainer": "falconindy",
        "FirstSubmitted": 1499013608,
        "LastModified": 1534000474,
        "URLPath": "/cgit/aur.git/snapshot/auracle-git.tar.gz",
        "Depends": [
          "pacman",
          "libarchive.so",
          "libcurl.so"
        ],
        "Groups": [
          "whydoestheaurhavegroups"
        ],
        "CheckDepends": [
          "python"
        ],
        "MakeDepends": [
          "meson",
          "git",
          "nlohmann-json"
        ],
        "Conflicts": [
          "auracle"
        ],
        "Provides": [
          "auracle"
        ],
        "License": [
          "MIT"
        ],
        "Replaces": [
          "cower-git",
          "cower"
        ],
        "OptDepends": [
          "awesomeness"
        ],
        "Keywords": [
          "aur"
        ]
      }
    ]
  })");

  ASSERT_TRUE(response.ok()) << response.status();
  ASSERT_EQ(response->packages.size(), 1);

  const auto& result = response->packages[0];
  EXPECT_EQ(result.package_id, 534056);
  EXPECT_EQ(result.name, "auracle-git");
  EXPECT_EQ(result.pkgbase_id, 123768);
  EXPECT_EQ(result.version, "r36.752e4ba-1");
  EXPECT_EQ(result.description, "A flexible client for the AUR");
  EXPECT_EQ(result.upstream_url, "https://github.com/falconindy/auracle.git");
  EXPECT_EQ(result.votes, 15);
  EXPECT_EQ(result.popularity, 0.095498);
  EXPECT_EQ(result.out_of_date, absl::UnixEpoch());
  EXPECT_EQ(result.submitted, absl::FromUnixSeconds(1499013608));
  EXPECT_EQ(result.modified, absl::FromUnixSeconds(1534000474));
  EXPECT_EQ(result.maintainer, "falconindy");
  EXPECT_EQ(result.aur_urlpath, "/cgit/aur.git/snapshot/auracle-git.tar.gz");
  EXPECT_THAT(result.depends,
              UnorderedElementsAre("pacman", "libarchive.so", "libcurl.so"));
  EXPECT_THAT(result.makedepends,
              UnorderedElementsAre("meson", "git", "nlohmann-json"));
  EXPECT_THAT(result.checkdepends, UnorderedElementsAre("python"));
  EXPECT_THAT(result.optdepends, UnorderedElementsAre("awesomeness"));
  EXPECT_THAT(result.conflicts, UnorderedElementsAre("auracle"));
  EXPECT_THAT(result.replaces, UnorderedElementsAre("cower", "cower-git"));
  EXPECT_THAT(result.provides, UnorderedElementsAre("auracle"));
  EXPECT_THAT(result.licenses, UnorderedElementsAre("MIT"));
  EXPECT_THAT(result.keywords, UnorderedElementsAre("aur"));
  EXPECT_THAT(result.groups, UnorderedElementsAre("whydoestheaurhavegroups"));
}

TEST(ResponseTest, ParsesErrorResponse) {
  const auto response = RpcResponse::Parse(R"({
    "version": 5,
    "type": "error",
    "resultcount": 0,
    "results": [],
    "error": "something"
  })");

  EXPECT_FALSE(response.ok());
  EXPECT_EQ(response.status().message(), "something");
}

TEST(ResponseTest, GracefullyHandlesInvalidJson) {
  const auto response = RpcResponse::Parse(R"({
    "version": 5,
    "type": "multiinfo,
    "resultcount": 0,
    "results": [],
    "error": "something"
  })");

  ASSERT_THAT(response.status().message(), testing::HasSubstr("parse error"));
}

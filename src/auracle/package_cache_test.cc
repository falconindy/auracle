#include "package_cache.hh"

#include "aur/package.hh"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::ElementsAre;
using testing::Field;
using testing::UnorderedElementsAre;

TEST(PackageCacheTest, AddsPackages) {
  aur::Package package(R"(
    {
      "ID": 534056,
      "Name": "auracle-git",
      "PackageBaseID": 123768,
      "PackageBase": "auracle-git"
    }
  )");

  auracle::PackageCache cache;
  ASSERT_TRUE(cache.empty());

  {
    auto [p, added] = cache.AddPackage(package);
    EXPECT_TRUE(added);
    EXPECT_EQ(*p, package);
    EXPECT_EQ(1, cache.size());
  }

  {
    auto [p, added] = cache.AddPackage(package);
    EXPECT_FALSE(added);
    EXPECT_EQ(*p, package);
    EXPECT_EQ(1, cache.size());
  }
}

TEST(PackageCacheTest, LooksUpPackages) {
  auracle::PackageCache cache;
  cache.AddPackage(aur::Package(R"(
    {
      "ID": 534056,
      "Name": "auracle-git",
      "PackageBaseID": 123768,
      "PackageBase": "auracle-git",
      "Version": "r36.752e4ba-1"
    }
  )"));
  cache.AddPackage(aur::Package(R"(
    {
      "ID": 534055,
      "Name": "pkgfile-git",
      "PackageBaseID": 60915,
      "PackageBase": "pkgfile-git",
      "Version": "18.2.g6e6150d-1"
    }
  )"));

  {
    auto result = cache.LookupByPkgbase("pkgfile-git");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->name, "pkgfile-git");
  }

  {
    auto result = cache.LookupByPkgname("auracle-git");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->name, "auracle-git");
  }

  EXPECT_EQ(cache.LookupByPkgbase("notfound-pkgbase"), nullptr);
  EXPECT_EQ(cache.LookupByPkgname("notfound-pkgname"), nullptr);
}

TEST(PackageCacheTest, WalkDependencies) {
  auracle::PackageCache cache;
  cache.AddPackage(aur::Package(R"(
    {
      "ID": 534055,
      "Name": "pkgfile-git",
      "PackageBaseID": 60915,
      "PackageBase": "pkgfile-git",
      "Version": "18.2.g6e6150d-1",
      "Description": "a pacman .files metadata explorer",
      "URL": "http://github.com/falconindy/pkgfile",
      "NumVotes": 42,
      "Popularity": 0.070083,
      "OutOfDate": null,
      "Maintainer": "falconindy",
      "FirstSubmitted": 1342485230,
      "LastModified": 1534000387,
      "URLPath": "/cgit/aur.git/snapshot/pkgfile-git.tar.gz",
      "Depends": [
        "libarchive",
        "curl",
        "pcre",
        "pacman-git"
      ]
    }
  )"));
  cache.AddPackage(aur::Package(R"(
    {
      "ID": 600011,
      "Name": "pacman-git",
      "PackageBaseID": 29937,
      "PackageBase": "pacman-git",
      "Version": "5.1.1.r160.gd37e6d40-2",
      "Description": "A library-based package manager with dependency support",
      "URL": "https://www.archlinux.org/pacman/",
      "NumVotes": 22,
      "Popularity": 0.327399,
      "OutOfDate": null,
      "Maintainer": "eschwartz",
      "FirstSubmitted": 1252344761,
      "LastModified": 1554053989,
      "URLPath": "/cgit/aur.git/snapshot/pacman-git.tar.gz",
      "Depends": [
        "archlinux-keyring",
        "bash",
        "curl",
        "gpgme",
        "libarchive",
        "pacman-mirrorlist"
      ],
      "MakeDepends": [
        "git",
        "asciidoc",
        "meson"
      ],
      "CheckDepends": [
        "python",
        "fakechroot"
      ]
    }
  )"));

  std::vector<std::string> walked_packages;
  std::vector<const aur::Package*> aur_packages;
  cache.WalkDependencies("pkgfile-git",
                         [&](const std::string& name, const aur::Package* pkg) {
                           walked_packages.push_back(name);
                           if (pkg != nullptr) {
                             aur_packages.push_back(pkg);
                           }
                         });

  EXPECT_THAT(
      walked_packages,
      ElementsAre("libarchive", "curl", "pcre", "archlinux-keyring", "bash",
                  "gpgme", "pacman-mirrorlist", "git", "asciidoc", "meson",
                  "python", "fakechroot", "pacman-git", "pkgfile-git"));

  EXPECT_THAT(aur_packages,
              UnorderedElementsAre(Field(&aur::Package::name, "pkgfile-git"),
                                   Field(&aur::Package::name, "pacman-git")));
}

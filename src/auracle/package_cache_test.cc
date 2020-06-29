// SPDX-License-Identifier: MIT
#include "package_cache.hh"

#include "aur/package.hh"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::ElementsAre;
using testing::Field;
using testing::UnorderedElementsAre;

TEST(PackageCacheTest, AddsPackages) {
  aur::Package package;
  package.package_id = 534056;
  package.name = "auracle-git";
  package.pkgbase_id = 123768;
  package.pkgbase = "auracle-git";

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

  {
    aur::Package package;
    package.package_id = 534056;
    package.name = "auracle-git";
    package.pkgbase_id = 123768;
    package.pkgbase = "auracle-git";
    cache.AddPackage(package);
  }

  {
    aur::Package package;
    package.package_id = 534055;
    package.name = "pkgfile-git";
    package.pkgbase_id = 60915;
    package.pkgbase = "pkgfile-git";
    cache.AddPackage(package);
  }

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

aur::Dependency MakeDependency(const std::string& name) {
  aur::Dependency d;
  d.name = name;
  d.depstring = name;

  return d;
}

TEST(PackageCacheTest, WalkDependencies) {
  auracle::PackageCache cache;
  {
    aur::Package package;
    package.package_id = 534055;
    package.name = "pkgfile-git";
    package.pkgbase_id = 60915;
    package.pkgbase = "pkgfile-git";
    package.depends = {MakeDependency("libarchive"), MakeDependency("curl"),
                       MakeDependency("pcre"), MakeDependency("pacman-git")};
    cache.AddPackage(package);
  }
  {
    aur::Package package;
    package.package_id = 600011;
    package.name = "pacman-git";
    package.pkgbase_id = 29937;
    package.pkgbase = "pacman-git";
    package.depends = {MakeDependency("archlinux-keyring"),
                       MakeDependency("bash"),
                       MakeDependency("curl"),
                       MakeDependency("gpgme"),
                       MakeDependency("libarchive"),
                       MakeDependency("pacman-mirrorlist")};
    package.makedepends = {MakeDependency("git"), MakeDependency("asciidoc"),
                           MakeDependency("meson")};
    package.checkdepends = {MakeDependency("python"),
                            MakeDependency("fakechroot")};
    cache.AddPackage(package);
  }

  std::vector<std::string> walked_packages;
  std::vector<const aur::Package*> aur_packages;
  cache.WalkDependencies(
      "pkgfile-git",
      [&](const std::string& name, const aur::Package* pkg,
          const std::vector<std::string>&) {
        walked_packages.push_back(name);
        if (pkg != nullptr) {
          aur_packages.push_back(pkg);
        }
      },
      std::set<auracle::DependencyKind>{
          auracle::DependencyKind::Depend,
          auracle::DependencyKind::CheckDepend,
          auracle::DependencyKind::MakeDepend,
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

TEST(PackageCacheTest, WalkDependenciesWithLimitedDeps) {
  auracle::PackageCache cache;
  {
    aur::Package package;
    package.package_id = 534055;
    package.name = "pkgfile-git";
    package.pkgbase_id = 60915;
    package.pkgbase = "pkgfile-git";
    package.depends = {MakeDependency("libarchive"), MakeDependency("curl"),
                       MakeDependency("pcre"), MakeDependency("pacman-git")};
    cache.AddPackage(package);
  }
  {
    aur::Package package;
    package.package_id = 600011;
    package.name = "pacman-git";
    package.pkgbase_id = 29937;
    package.pkgbase = "pacman-git";
    package.depends = {MakeDependency("archlinux-keyring"),
                       MakeDependency("bash"),
                       MakeDependency("curl"),
                       MakeDependency("gpgme"),
                       MakeDependency("libarchive"),
                       MakeDependency("pacman-mirrorlist")};
    package.makedepends = {MakeDependency("git"), MakeDependency("asciidoc"),
                           MakeDependency("meson")};
    package.checkdepends = {MakeDependency("python"),
                            MakeDependency("fakechroot")};
    cache.AddPackage(package);
  }

  std::vector<std::string> walked_packages;
  std::vector<const aur::Package*> aur_packages;

  auto walk_dependencies_fn = [&](const std::string& name,
                                  const aur::Package* pkg,
                                  const std::vector<std::string>&) {
    walked_packages.push_back(name);
    if (pkg != nullptr) {
      aur_packages.push_back(pkg);
    }
  };

  cache.WalkDependencies("pkgfile-git", walk_dependencies_fn,
                         std::set<auracle::DependencyKind>{
                             auracle::DependencyKind::Depend,
                             auracle::DependencyKind::MakeDepend,
                         });
  EXPECT_THAT(walked_packages,
              ElementsAre("libarchive", "curl", "pcre", "archlinux-keyring",
                          "bash", "gpgme", "pacman-mirrorlist", "git",
                          "asciidoc", "meson", "pacman-git", "pkgfile-git"));
  walked_packages.clear();
  aur_packages.clear();

  cache.WalkDependencies("pkgfile-git", walk_dependencies_fn,
                         std::set<auracle::DependencyKind>{
                             auracle::DependencyKind::Depend,
                         });
  EXPECT_THAT(
      walked_packages,
      ElementsAre("libarchive", "curl", "pcre", "archlinux-keyring", "bash",
                  "gpgme", "pacman-mirrorlist", "pacman-git", "pkgfile-git"));
  walked_packages.clear();
  aur_packages.clear();

  cache.WalkDependencies("pacman-git", walk_dependencies_fn,
                         std::set<auracle::DependencyKind>{
                             auracle::DependencyKind::CheckDepend,
                         });
  EXPECT_THAT(walked_packages,
              ElementsAre("python", "fakechroot", "pacman-git"));
  walked_packages.clear();
  aur_packages.clear();
}

// SPDX-License-Identifier: MIT
#include "auracle/package_cache.hh"

#include "aur/package.hh"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using auracle::Dependency;
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

std::string MakeDependency(const std::string& name) { return name; }

TEST(PackageCacheTest, WalkDependencies) {
  auracle::PackageCache cache;
  {
    aur::Package package;
    package.package_id = 534055;
    package.name = "pkgfile-git";
    package.pkgbase_id = 60915;
    package.pkgbase = "pkgfile-git";
    package.depends = {"libarchive", "curl", "pcre", "pacman-git"};
    cache.AddPackage(package);
  }
  {
    aur::Package package;
    package.package_id = 600011;
    package.name = "pacman-git";
    package.pkgbase_id = 29937;
    package.pkgbase = "pacman-git";
    package.depends = {
        "archlinux-keyring", "bash", "curl", "gpgme", "libarchive",
        "pacman-mirrorlist"};
    package.makedepends = {"git", "asciidoc", "meson"};
    package.checkdepends = {"python", "fakechroot"};
    cache.AddPackage(package);
  }

  std::vector<std::string> walked_packages;
  std::vector<const aur::Package*> aur_packages;
  cache.WalkDependencies(
      "pkgfile-git",
      [&](const Dependency& dep, const aur::Package* pkg,
          const std::vector<std::string>&) {
        walked_packages.push_back(dep.name());
        if (pkg != nullptr) {
          aur_packages.push_back(pkg);
        }
      },
      absl::btree_set<auracle::DependencyKind>{
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
    package.depends = {"libarchive", "curl", "pcre", "pacman-git"};
    cache.AddPackage(package);
  }
  {
    aur::Package package;
    package.package_id = 600011;
    package.name = "pacman-git";
    package.pkgbase_id = 29937;
    package.pkgbase = "pacman-git";
    package.depends = {
        "archlinux-keyring", "bash", "curl", "gpgme", "libarchive",
        "pacman-mirrorlist"};
    package.makedepends = {"git", "asciidoc", "meson"};
    package.checkdepends = {"python", "fakechroot"};
    cache.AddPackage(package);
  }

  std::vector<std::string> walked_packages;
  std::vector<const aur::Package*> aur_packages;

  auto walk_dependencies_fn = [&](const Dependency& dep,
                                  const aur::Package* pkg,
                                  const std::vector<std::string>&) {
    walked_packages.push_back(dep.name());
    if (pkg != nullptr) {
      aur_packages.push_back(pkg);
    }
  };

  cache.WalkDependencies("pkgfile-git", walk_dependencies_fn,
                         absl::btree_set<auracle::DependencyKind>{
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
                         absl::btree_set<auracle::DependencyKind>{
                             auracle::DependencyKind::Depend,
                         });
  EXPECT_THAT(
      walked_packages,
      ElementsAre("libarchive", "curl", "pcre", "archlinux-keyring", "bash",
                  "gpgme", "pacman-mirrorlist", "pacman-git", "pkgfile-git"));
  walked_packages.clear();
  aur_packages.clear();

  cache.WalkDependencies("pacman-git", walk_dependencies_fn,
                         absl::btree_set<auracle::DependencyKind>{
                             auracle::DependencyKind::CheckDepend,
                         });
  EXPECT_THAT(walked_packages,
              ElementsAre("python", "fakechroot", "pacman-git"));
  walked_packages.clear();
  aur_packages.clear();
}

TEST(PackageCacheTest, FindDependencySatisfiers) {
  auracle::PackageCache cache;
  {
    aur::Package package;
    package.package_id = 1;
    package.name = "pkgfile-super-shiny";
    package.pkgbase = "pkgfile-super-shiny";
    package.provides = {"pkgfile=11"};
    cache.AddPackage(package);
  }
  {
    aur::Package package;
    package.package_id = 2;
    package.name = "pkgfile-git";
    package.pkgbase = "pkgfile-git";
    package.provides = {"pkgfile=10"};
    cache.AddPackage(package);
  }
  {
    aur::Package package;
    package.package_id = 3;
    package.name = "pacman-git";
    package.pkgbase = "pacman-git";
    package.provides = {"pacman=6.1.0"};
    cache.AddPackage(package);
  }

  EXPECT_THAT(
      cache.FindDependencySatisfiers(Dependency("pkgfile")),
      UnorderedElementsAre(Field(&aur::Package::name, "pkgfile-git"),
                           Field(&aur::Package::name, "pkgfile-super-shiny")));

  EXPECT_THAT(cache.FindDependencySatisfiers(Dependency("pkgfile=10")),
              UnorderedElementsAre(Field(&aur::Package::name, "pkgfile-git")));

  EXPECT_THAT(cache.FindDependencySatisfiers(Dependency("pacman>6.1.0")),
              UnorderedElementsAre());
}

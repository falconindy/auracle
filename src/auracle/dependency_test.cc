#include "auracle/dependency.hh"

#include "gtest/gtest.h"

using aur::Package;
using auracle::Dependency;

namespace {

TEST(DependencyTest, UnversionedRequirement) {
  Package foo;
  foo.name = "foo";
  foo.version = "1.0.0";

  Package bar;
  bar.name = "bar";
  bar.version = "1.0.0";

  Dependency dep("foo");
  EXPECT_TRUE(dep.SatisfiedBy(foo));
  EXPECT_FALSE(dep.SatisfiedBy(bar));
}

TEST(DependencyTest, VersionedRequirement) {
  Package foo_0_9_9;
  foo_0_9_9.name = "foo";
  foo_0_9_9.version = "0.9.9";

  Package foo_1_0_0;
  foo_1_0_0.name = "foo";
  foo_1_0_0.version = "1.0.0";

  Package foo_1_1_0;
  foo_1_1_0.name = "foo";
  foo_1_1_0.version = "1.1.0";

  Package bar_1_0_0;
  bar_1_0_0.name = "bar";
  bar_1_0_0.version = "1.0.0";

  {
    Dependency dep("foo=1.0.0");
    EXPECT_TRUE(dep.SatisfiedBy(foo_1_0_0));
    EXPECT_FALSE(dep.SatisfiedBy(foo_1_1_0));
  }

  {
    Dependency dep("foo>=1.0.0");
    EXPECT_FALSE(dep.SatisfiedBy(foo_0_9_9));
    EXPECT_TRUE(dep.SatisfiedBy(foo_1_0_0));
    EXPECT_TRUE(dep.SatisfiedBy(foo_1_1_0));
  }

  {
    Dependency dep("foo>1.0.0");
    EXPECT_FALSE(dep.SatisfiedBy(foo_0_9_9));
    EXPECT_FALSE(dep.SatisfiedBy(foo_1_0_0));
    EXPECT_TRUE(dep.SatisfiedBy(foo_1_1_0));
  }

  {
    Dependency dep("foo<=1.0.0");
    EXPECT_TRUE(dep.SatisfiedBy(foo_0_9_9));
    EXPECT_TRUE(dep.SatisfiedBy(foo_1_0_0));
    EXPECT_FALSE(dep.SatisfiedBy(foo_1_1_0));
  }

  {
    Dependency dep("foo<1.0.0");
    EXPECT_TRUE(dep.SatisfiedBy(foo_0_9_9));
    EXPECT_FALSE(dep.SatisfiedBy(foo_1_0_0));
    EXPECT_FALSE(dep.SatisfiedBy(foo_1_1_0));
  }

  {
    Dependency dep("foo=1.0.0");
    EXPECT_FALSE(dep.SatisfiedBy(bar_1_0_0));
  }
}

TEST(DependencyTest, UnversionedRequirementByProvision) {
  Package bar;
  bar.name = "bar";
  bar.version = "9.9.9";
  bar.provides.push_back("quux");
  bar.provides.push_back("foo");

  Package bar_2;
  bar_2.name = "bar";
  bar_2.version = "9.9.9";
  bar_2.provides.push_back("quux");
  bar_2.provides.push_back("foo=42");

  Dependency dep("foo");
  EXPECT_TRUE(dep.SatisfiedBy(bar));
  EXPECT_TRUE(dep.SatisfiedBy(bar_2));
}

TEST(DependencyTest, VersionedRequirementByProvision) {
  Package bar_0_9_9;
  bar_0_9_9.name = "bar";
  bar_0_9_9.version = "9.9.9";
  bar_0_9_9.provides.push_back("quux");
  bar_0_9_9.provides.push_back("foo=0.9.9");

  Package bar_1_0_0;
  bar_1_0_0.name = "bar";
  bar_1_0_0.version = "9.9.9";
  bar_0_9_9.provides.push_back("quux");
  bar_1_0_0.provides.push_back("foo=1.0.0");

  Package bar_1_1_0;
  bar_1_1_0.name = "bar";
  bar_1_1_0.version = "9.9.9";
  bar_0_9_9.provides.push_back("quux");
  bar_1_1_0.provides.push_back("foo=1.1.0");

  {
    Dependency dep("foo=1.0.0");
    EXPECT_TRUE(dep.SatisfiedBy(bar_1_0_0));
    EXPECT_FALSE(dep.SatisfiedBy(bar_1_1_0));
  }

  {
    Dependency dep("foo>=1.0.0");
    EXPECT_FALSE(dep.SatisfiedBy(bar_0_9_9));
    EXPECT_TRUE(dep.SatisfiedBy(bar_1_0_0));
    EXPECT_TRUE(dep.SatisfiedBy(bar_1_1_0));
  }

  {
    Dependency dep("foo>1.0.0");
    EXPECT_FALSE(dep.SatisfiedBy(bar_0_9_9));
    EXPECT_FALSE(dep.SatisfiedBy(bar_1_0_0));
    EXPECT_TRUE(dep.SatisfiedBy(bar_1_1_0));
  }

  {
    Dependency dep("foo<=1.0.0");
    EXPECT_TRUE(dep.SatisfiedBy(bar_0_9_9));
    EXPECT_TRUE(dep.SatisfiedBy(bar_1_0_0));
    EXPECT_FALSE(dep.SatisfiedBy(bar_1_1_0));
  }

  {
    Dependency dep("foo<1.0.0");
    EXPECT_TRUE(dep.SatisfiedBy(bar_0_9_9));
    EXPECT_FALSE(dep.SatisfiedBy(bar_1_0_0));
    EXPECT_FALSE(dep.SatisfiedBy(bar_1_1_0));
  }
}

TEST(DependencyTest, MalformedProvider) {
  Package foo;
  foo.provides.push_back("bar>=9");

  Dependency dep("bar=9");
  EXPECT_FALSE(dep.SatisfiedBy(foo));
}

}  // namespace

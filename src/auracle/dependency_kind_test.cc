// SPDX-License-Identifier: MIT
#include "auracle/auracle.hh"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace auracle {

void PrintTo(DependencyKind kind, std::ostream* os) {
  switch (kind) {
    case DependencyKind::Depend:
      *os << "Depend";
      break;
    case DependencyKind::MakeDepend:
      *os << "MakeDepend";
      break;
    case DependencyKind::CheckDepend:
      *os << "CheckDepend";
      break;
  }
}

}  // namespace auracle

namespace {

TEST(AuracleTest, ParseDependencyKinds) {
  using DK = auracle::DependencyKind;

  {
    // Overwrite
    absl::btree_set<DK> kinds;
    EXPECT_TRUE(ParseDependencyKinds("depends", &kinds));
    EXPECT_THAT(kinds, testing::UnorderedElementsAre(DK::Depend));
    EXPECT_TRUE(
        ParseDependencyKinds("depends,checkdepends,makedepends", &kinds));
    EXPECT_THAT(kinds, testing::UnorderedElementsAre(
                           DK::Depend, DK::CheckDepend, DK::MakeDepend));
  }

  {
    // Remove
    absl::btree_set<DK> kinds = {DK::Depend, DK::MakeDepend, DK::CheckDepend};
    EXPECT_TRUE(ParseDependencyKinds("^checkdepends", &kinds));
    EXPECT_THAT(kinds,
                testing::UnorderedElementsAre(DK::Depend, DK::MakeDepend));
    EXPECT_TRUE(ParseDependencyKinds("!depends,makedepends", &kinds));
    EXPECT_THAT(kinds, testing::IsEmpty());
  }

  {
    // Append
    absl::btree_set<DK> kinds;
    EXPECT_TRUE(ParseDependencyKinds("+depends", &kinds));
    EXPECT_THAT(kinds, testing::UnorderedElementsAre(DK::Depend));
    EXPECT_TRUE(ParseDependencyKinds("+makedepends,checkdepends", &kinds));
    EXPECT_THAT(kinds, testing::UnorderedElementsAre(DK::Depend, DK::MakeDepend,
                                                     DK::CheckDepend));
  }

  {
    // Bad spelling
    absl::btree_set<DK> kinds = {DK::Depend};
    EXPECT_FALSE(ParseDependencyKinds("derpends", &kinds));
    EXPECT_THAT(kinds, testing::UnorderedElementsAre(DK::Depend));
  }

  {
    // Negation in the middle of a string isn't allowed
    absl::btree_set<DK> kinds = {DK::Depend};
    EXPECT_FALSE(ParseDependencyKinds("depends,!makedepends", &kinds));
    EXPECT_THAT(kinds, testing::UnorderedElementsAre(DK::Depend));
  }

  {
    // Bad second element still leaves out param untouched.
    absl::btree_set<DK> kinds = {DK::Depend};
    EXPECT_FALSE(ParseDependencyKinds("depends,!makdepends", &kinds));
    EXPECT_THAT(kinds, testing::UnorderedElementsAre(DK::Depend));
  }

  {
    // Edge case of only a valid prefix
    absl::btree_set<DK> kinds;
    EXPECT_FALSE(ParseDependencyKinds("+", &kinds));
    EXPECT_FALSE(ParseDependencyKinds("!", &kinds));
  }
}

}  // namespace

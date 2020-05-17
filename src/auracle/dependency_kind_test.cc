#include "auracle.hh"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

TEST(AuracleTest, ParseDependencyKinds) {
  using DK = auracle::DependencyKind;

  std::set<DK> kinds;

  // Overwrite
  EXPECT_TRUE(ParseDependencyKinds("depends", &kinds));
  EXPECT_THAT(kinds, testing::UnorderedElementsAre(DK::Depend));
  EXPECT_TRUE(ParseDependencyKinds("depends,checkdepends,makedepends", &kinds));
  EXPECT_THAT(kinds, testing::UnorderedElementsAre(DK::Depend, DK::CheckDepend,
                                                   DK::MakeDepend));

  // Remove
  EXPECT_TRUE(ParseDependencyKinds("^checkdepends", &kinds));
  EXPECT_THAT(kinds, testing::UnorderedElementsAre(DK::Depend, DK::MakeDepend));
  EXPECT_TRUE(ParseDependencyKinds("!depends,makedepends", &kinds));
  EXPECT_THAT(kinds, testing::IsEmpty());

  // Append
  EXPECT_TRUE(ParseDependencyKinds("+depends", &kinds));
  EXPECT_THAT(kinds, testing::UnorderedElementsAre(DK::Depend));
  EXPECT_TRUE(ParseDependencyKinds("+makedepends,checkdepends", &kinds));
  EXPECT_THAT(kinds, testing::UnorderedElementsAre(DK::Depend, DK::MakeDepend,
                                                   DK::CheckDepend));

  // Bad spelling
  kinds = {DK::Depend};
  EXPECT_FALSE(ParseDependencyKinds("derpends", &kinds));
  EXPECT_THAT(kinds, testing::UnorderedElementsAre(DK::Depend));

  // Negation in the middle of a string isn't allowed
  EXPECT_FALSE(ParseDependencyKinds("depends,!makedepends", &kinds));
  EXPECT_THAT(kinds, testing::UnorderedElementsAre(DK::Depend));

  // Bad second element still leaves out param untouched.
  EXPECT_FALSE(ParseDependencyKinds("depends,!makdepends", &kinds));
  EXPECT_THAT(kinds, testing::UnorderedElementsAre(DK::Depend));
}

}  // namespace

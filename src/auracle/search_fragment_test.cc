// SPDX-License-Identifier: MIT
#include "search_fragment.hh"

#include "gtest/gtest.h"

std::string GetSearchFragment(std::string_view input) {
  // In case we create a non-terminating loop in GetSearchFragment, this output
  // is helpful to understand what test case we're stuck on.
  printf("input: %s\n", std::string(input).c_str());

  // Explanations for string_views aren't fantastic in gtest because the data
  // isn't guaranteed to be non-binary or null-terminated. We know better, so
  // coerce the return value to a string.
  return std::string(auracle::GetSearchFragment(input));
}

TEST(SearchFragmentTest, ExtractsSuitableFragment) {
  EXPECT_EQ("foobar", GetSearchFragment("foobar"));
  EXPECT_EQ("foobar", GetSearchFragment("foobar$"));
  EXPECT_EQ("foobar", GetSearchFragment("^foobar"));

  EXPECT_EQ("foobar", GetSearchFragment("[invalid]foobar"));
  EXPECT_EQ("foobar", GetSearchFragment("foobar[invalid]"));
  EXPECT_EQ("moobarbaz", GetSearchFragment("foobar[invalid]moobarbaz"));

  EXPECT_EQ("foobar", GetSearchFragment("{invalid}foobar"));
  EXPECT_EQ("foobar", GetSearchFragment("foobar{invalid}"));
  EXPECT_EQ("moobarbaz", GetSearchFragment("foobar{invalid}moobarbaz"));

  EXPECT_EQ("co", GetSearchFragment("cow?fu"));
  EXPECT_EQ("fun", GetSearchFragment("co*fun"));

  EXPECT_EQ("co", GetSearchFragment("cow?fu?"));
  EXPECT_EQ("fu", GetSearchFragment("co*fun*"));

  EXPECT_EQ("foo", GetSearchFragment("fooo*"));
  EXPECT_EQ("foo", GetSearchFragment("fooo?"));
  EXPECT_EQ("fooo", GetSearchFragment("fooo+"));

  EXPECT_EQ("foo", GetSearchFragment("(foo|bar)"));
  EXPECT_EQ("foooo", GetSearchFragment("vim.*(foooo|barr)"));

  EXPECT_EQ("foobar",
            GetSearchFragment("^[derp]foobar[[inva$lid][{]}moo?bar{b}az"));

  EXPECT_EQ("", GetSearchFragment("[foobar]"));
  EXPECT_EQ("", GetSearchFragment("{foobar}"));
  EXPECT_EQ("", GetSearchFragment("{foobar"));
  EXPECT_EQ("", GetSearchFragment("foo[bar"));
  EXPECT_EQ("", GetSearchFragment("f+"));
  EXPECT_EQ("", GetSearchFragment("f+o+o+b+a+r"));
}

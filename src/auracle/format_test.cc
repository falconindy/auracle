// SPDX-License-Identifier: MIT
#include "auracle/format.hh"

#include <iostream>
#include <sstream>

#include "aur/package.hh"
#include "gtest/gtest.h"

class ScopedCapturer {
 public:
  ScopedCapturer(std::ostream& stream) : stream_(stream) {
    stream_.rdbuf(buffer_.rdbuf());
  }

  ~ScopedCapturer() { stream_.rdbuf(original_sbuf_); }

  std::string GetCapturedOutput() {
    auto str = buffer_.str();
    buffer_.str(std::string());
    return str;
  }

 private:
  std::stringstream buffer_;
  std::streambuf* original_sbuf_ = std::cout.rdbuf();
  std::ostream& stream_;
};

aur::Package MakePackage() {
  aur::Package p;

  // string
  p.name = "cower";
  p.version = "1.2.3";

  // floating point
  p.popularity = 5.20238;

  // datetime
  p.submitted = absl::FromUnixSeconds(1499013608);

  // lists
  p.conflicts = {
      "auracle",
      "cower",
      "cower-git",
  };

  return p;
}

TEST(FormatTest, DetectsInvalidFormats) {
  EXPECT_FALSE(format::Validate("{invalid}").ok());
}

TEST(FormatTest, CustomStringFormat) {
  ScopedCapturer capture(std::cout);

  format::Custom("{name} -> {version}", MakePackage());

  EXPECT_EQ(capture.GetCapturedOutput(), "cower -> 1.2.3\n");
}

TEST(FormatTest, CustomFloatFormat) {
  ScopedCapturer capture(std::cout);

  auto p = MakePackage();

  format::Custom("{popularity}", p);
  EXPECT_EQ(capture.GetCapturedOutput(), "5.20238\n");

  format::Custom("{popularity:.2f}", p);
  EXPECT_EQ(capture.GetCapturedOutput(), "5.20\n");
}

TEST(FormatTest, CustomDateTimeFormat) {
  ScopedCapturer capture(std::cout);

  auto p = MakePackage();

  format::Custom("{submitted}", p);
  EXPECT_EQ(capture.GetCapturedOutput(), "2017-07-02T16:40:08+00:00\n");

  format::Custom("{submitted:%s}", p);
  EXPECT_EQ(capture.GetCapturedOutput(), "1499013608\n");
}

TEST(FormatTest, ListFormat) {
  ScopedCapturer capture(std::cout);

  auto p = MakePackage();

  format::Custom("{conflicts}", p);
  EXPECT_EQ(capture.GetCapturedOutput(), "auracle  cower  cower-git\n");

  format::Custom("{conflicts::,,}", p);
  EXPECT_EQ(capture.GetCapturedOutput(), "auracle:,,cower:,,cower-git\n");
}

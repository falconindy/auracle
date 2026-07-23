// SPDX-License-Identifier: MIT
#include "auracle/format.hh"

#include "aur/package.hh"
#include "gtest/gtest.h"

class ScopedStdoutCapturer {
 public:
  ScopedStdoutCapturer() { testing::internal::CaptureStdout(); }

  std::string GetCapturedOutput() {
    return testing::internal::GetCapturedStdout();
  }
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
  ScopedStdoutCapturer capture;

  format::Custom("{name} -> {version}", MakePackage());

  EXPECT_EQ(capture.GetCapturedOutput(), "cower -> 1.2.3\n");
}

TEST(FormatTest, CustomFloatFormat) {
  auto p = MakePackage();

  {
    ScopedStdoutCapturer capture;
    format::Custom("{popularity}", p);
    EXPECT_EQ(capture.GetCapturedOutput(), "5.20238\n");
  }

  {
    ScopedStdoutCapturer capture;
    format::Custom("{popularity:.2f}", p);
    EXPECT_EQ(capture.GetCapturedOutput(), "5.20\n");
  }
}

TEST(FormatTest, LongFormatDoesNotUseScientificNotationForPopularity) {
  auto p = MakePackage();
  p.popularity = 4.7e-05;

  ScopedStdoutCapturer capture;
  format::Long(p, std::nullopt);

  const auto output = capture.GetCapturedOutput();
  EXPECT_NE(output.find("Popularity     : 0.000047"), std::string::npos)
      << output;
  EXPECT_EQ(output.find("e-05"), std::string::npos) << output;
}

TEST(FormatTest, ShortFormatDoesNotUseScientificNotationForPopularity) {
  auto p = MakePackage();
  p.popularity = 4.7e-05;

  ScopedStdoutCapturer capture;
  format::Short(p, std::nullopt);

  const auto output = capture.GetCapturedOutput();
  EXPECT_NE(output.find("0.000047"), std::string::npos) << output;
  EXPECT_EQ(output.find("e-05"), std::string::npos) << output;
}

TEST(FormatTest, CustomDateTimeFormat) {
  auto p = MakePackage();

  {
    ScopedStdoutCapturer capture;
    format::Custom("{submitted}", p);
    EXPECT_EQ(capture.GetCapturedOutput(), "2017-07-02T16:40:08+00:00\n");
  }

  {
    ScopedStdoutCapturer capture;
    format::Custom("{submitted:%s}", p);
    EXPECT_EQ(capture.GetCapturedOutput(), "1499013608\n");
  }
}

TEST(FormatTest, ListFormat) {
  auto p = MakePackage();

  {
    ScopedStdoutCapturer capture;
    format::Custom("{conflicts}", p);
    EXPECT_EQ(capture.GetCapturedOutput(), "auracle  cower  cower-git\n");
  }

  {
    ScopedStdoutCapturer capture;
    format::Custom("{conflicts::,,}", p);
    EXPECT_EQ(capture.GetCapturedOutput(), "auracle:,,cower:,,cower-git\n");
  }
}

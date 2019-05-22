#include "sort.hh"

#include <vector>

#include "aur/package.hh"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::ElementsAre;
using testing::Field;

TEST(SortOrderTest, RejectsInvalidSortField) {
  EXPECT_EQ(nullptr, sort::MakePackageSorter("", sort::OrderBy::ORDER_ASC));
  EXPECT_EQ(nullptr,
            sort::MakePackageSorter("invalid", sort::OrderBy::ORDER_ASC));
  EXPECT_EQ(nullptr,
            sort::MakePackageSorter("depends", sort::OrderBy::ORDER_ASC));
}

std::vector<aur::Package> MakePackages() {
  std::vector<aur::Package> packages;

  packages.emplace_back(R"(
      {
        "Name": "cower",
        "Popularity": 1.2345,
        "NumVotes": 30,
        "FirstSubmitted": 10000,
        "LastModified": 20000
      }
    )");
  packages.emplace_back(R"(
      {
        "Name": "auracle",
        "Popularity": 5.3241,
        "NumVotes": 20,
        "FirstSubmitted": 20000,
        "LastModified": 40000
      }
    )");
  packages.emplace_back(R"(
      {
        "Name": "pacman",
        "Popularity": 5.3240,
        "NumVotes": 10,
        "FirstSubmitted": 30000,
        "LastModified": 10000
      }
    )");

  return packages;
}

class SortOrderTest : public testing::TestWithParam<sort::OrderBy> {
 protected:
  SortOrderTest() : packages_(MakePackages()) {}

  template <typename ContainerMatcher>
  void ExpectSorted(std::string_view field, ContainerMatcher matcher) {
    std::sort(packages_.begin(), packages_.end(),
              sort::MakePackageSorter(field, GetParam()));

    // lolhacky, but there's no easy way to express the reverse order of the
    // matchers without repeating it in the tests?
    if (GetParam() == sort::OrderBy::ORDER_DESC) {
      std::reverse(packages_.begin(), packages_.end());
    }

    EXPECT_THAT(packages_, matcher);
  }

 private:
  std::vector<aur::Package> packages_;
};

TEST_P(SortOrderTest, ByName) {
  ExpectSorted("name", ElementsAre(Field(&aur::Package::name, "auracle"),
                                   Field(&aur::Package::name, "cower"),
                                   Field(&aur::Package::name, "pacman")));
}

TEST_P(SortOrderTest, ByPopularity) {
  ExpectSorted("popularity",
               ElementsAre(Field(&aur::Package::popularity, 1.2345),
                           Field(&aur::Package::popularity, 5.3240),
                           Field(&aur::Package::popularity, 5.3241)));
}

TEST_P(SortOrderTest, ByVotes) {
  ExpectSorted("votes", ElementsAre(Field(&aur::Package::votes, 10),
                                    Field(&aur::Package::votes, 20),
                                    Field(&aur::Package::votes, 30)));
}

TEST_P(SortOrderTest, ByFirstSubmitted) {
  using s = std::chrono::seconds;
  ExpectSorted("firstsubmitted",
               ElementsAre(Field(&aur::Package::submitted_s, s(10000)),
                           Field(&aur::Package::submitted_s, s(20000)),
                           Field(&aur::Package::submitted_s, s(30000))));
}

TEST_P(SortOrderTest, ByLastModified) {
  using s = std::chrono::seconds;
  ExpectSorted("lastmodified",
               ElementsAre(Field(&aur::Package::modified_s, s(10000)),
                           Field(&aur::Package::modified_s, s(20000)),
                           Field(&aur::Package::modified_s, s(40000))));
}

INSTANTIATE_TEST_CASE_P(BothOrderings, SortOrderTest,
                        testing::Values(sort::OrderBy::ORDER_ASC,
                                        sort::OrderBy::ORDER_DESC));

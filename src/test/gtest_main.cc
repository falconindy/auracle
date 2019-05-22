#include <unistd.h>

#include "gtest/gtest.h"

int main(int argc, char** argv) {
  setenv("TZ", "UTC", 1);
  setenv("LC_TIME", "C", 1);

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#include <gtest/gtest.h>

#pragma comment( lib, "gtestd.lib" )
#pragma comment( lib, "gtest_maind.lib" )

TEST(GTestSample, Assert) {
    EXPECT_EQ(1, 1);
    EXPECT_EQ(2, 3);
}
#pragma once

#include <gtest/gtest.h>

#include "../main/util.h"
#include "../main/fol.h"

using namespace dav;


TEST(UtilityTest, String)
{
    string_t s1("xYZYYz");

    EXPECT_EQ(s1.lower(), "xyzyyz");

    EXPECT_EQ(s1.split("Y").size(), 3);
    EXPECT_EQ(s1.split("Y", 1).size(), 2);
    EXPECT_EQ(s1.split("Y", 1).back(), "ZYYz");

    EXPECT_EQ(s1.strip("xz"), "YZYY");
    EXPECT_EQ(s1.strip("zxY"), "Z");
    EXPECT_EQ(s1.replace("YZ", "ab"), "xab");

    EXPECT_TRUE(s1.startswith("xYZ"));
    EXPECT_FALSE(s1.startswith("Zx"));

    EXPECT_TRUE(s1.endswith("YYz"));
    EXPECT_FALSE(s1.endswith("xz"));
}


TEST(UtilityTest, StringHash)
{
    string_hash_t x("x"), X("X"), Y("Y");
    string_hash_t u(string_hash_t::get_unknown_hash());

    EXPECT_TRUE(X.is_constant());
    EXPECT_TRUE(not x.is_constant());
    EXPECT_TRUE(not u.is_constant());

    EXPECT_TRUE(u.is_unknown());
    EXPECT_TRUE(not x.is_unknown());

    EXPECT_TRUE(x.is_unifiable_with(X));
    EXPECT_TRUE(x.is_unifiable_with(u));
    EXPECT_FALSE(X.is_unifiable_with(Y));
}


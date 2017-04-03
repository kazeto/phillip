#pragma once

#include <gtest/gtest.h>

#include "../main/util.h"

using namespace dav;


TEST(UtilityTest, String)
{
    string_t s("xYZYYz");

    EXPECT_EQ(s.lower(), "xyzyyz");

    EXPECT_EQ(s.split("Y").size(), 3);
    EXPECT_EQ(s.split("Y", 1).size(), 2);
    EXPECT_EQ(s.split("Y", 1).back(), "ZYYz");

    EXPECT_EQ(s.strip("xz"), "YZYY");
    EXPECT_EQ(s.strip("zxY"), "Z");
    EXPECT_EQ(s.replace("YZ", "ab"), "xabYYz");

    EXPECT_TRUE(s.startswith("xYZ"));
    EXPECT_FALSE(s.startswith("Zx"));

    EXPECT_TRUE(s.endswith("YYz"));
    EXPECT_FALSE(s.endswith("xz"));
}


TEST(UtilityTest, Filepath)
{
	filepath_t s("/aaa/bbb/ccc.txt");

	EXPECT_EQ(s.filename(), "ccc.txt");
	EXPECT_EQ(s.dirname(), filepath_t("/aaa/bbb"));
}


TEST(UtilityTest, StringHash)
{
    string_hash_t x("x"), X("X"), Y("Y");
	string_hash_t s("\"hello!\"");
	string_hash_t ux("___x"), uX("___X");
    string_hash_t u(string_hash_t::get_unknown_hash());

    EXPECT_TRUE(X.is_constant());
    EXPECT_TRUE(x.is_variable());
	EXPECT_TRUE(s.is_constant());
	EXPECT_TRUE(ux.is_variable());
	EXPECT_TRUE(uX.is_constant());
	EXPECT_TRUE(u.is_variable());

    EXPECT_TRUE(u.is_unknown());
    EXPECT_TRUE(not x.is_unknown());

    EXPECT_TRUE(x.is_unifiable_with(X));
    EXPECT_TRUE(x.is_unifiable_with(u));
    EXPECT_FALSE(X.is_unifiable_with(Y));
}


TEST(UtilityTest, ParameterStrage)
{
	param()->add("aaa", "xxx");
	param()->add("bbb", "123");
	param()->add("ccc", "12.4");

	EXPECT_TRUE(param()->has("aaa"));
	EXPECT_FALSE(param()->has("xxx"));

	EXPECT_EQ(param()->get("aaa"), "xxx");
	EXPECT_EQ(param()->geti("bbb"), 123);
	EXPECT_FLOAT_EQ(param()->getf("ccc"), 12.4);
	EXPECT_EQ(param()->geti("ccc"), 12);
	EXPECT_EQ(param()->get("ddd"), "");
	EXPECT_EQ(param()->get("ddd", "xxx"), "xxx");
	EXPECT_EQ(param()->geti("ddd"), -1);
}


TEST(UtilityTest, Binary)
{
	char buffer[256];
	binary_writer_t wr(buffer, 256);
	binary_reader_t rd(buffer, 256);

	size_t x = 123, y = 0;
	wr.write<size_t>(123);
	rd.read<size_t>(&y);

	EXPECT_EQ(y, 123);
	EXPECT_EQ(wr.size(), sizeof(size_t));
	EXPECT_EQ(rd.size(), sizeof(size_t));
}


TEST(UtilityTest, Others)
{
	std::vector<std::string> strs1{ "aa", "bb", "cc", "dd" };
	std::vector<std::string> strs2{ "aa", "bb", "cc", "dd" };

	auto joined = join(strs1.begin(), strs1.end(), " | ");
	EXPECT_EQ(joined, "aa | bb | cc | dd");

	EXPECT_TRUE(has_intersection(strs1.begin(), strs1.end(), strs2.begin(), strs2.end()));

	std::pair<int, int> p(1, 2);
	EXPECT_TRUE(symmetric_pair<int>(1, 2) == p);
	EXPECT_TRUE(symmetric_pair<int>(2, 1) == p);
}
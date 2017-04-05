#pragma once

#include <gtest/gtest.h>
#include <sstream>

#include "../main/parser.h"

using namespace dav;


TEST(ParserTest, Stream)
{
	string_t str =
		"# this is comment.\n"
		"      \n"
		"\'this is a quotation.\'\n"
		"\"this is a quotation, too.\"\n"
		"problem";
	parse::stream_t st(new std::istringstream(str));

	ASSERT_EQ(st.row(), 1);
	ASSERT_EQ(st.column(), 1);

	auto mistake = st.read(parse::predicate);
	ASSERT_EQ(mistake, "");
	ASSERT_EQ(st.row(), 1);
	ASSERT_EQ(st.column(), 1);

	auto comment = st.read(parse::comment);
	ASSERT_EQ(comment, "# this is comment.\n");
	ASSERT_EQ(st.row(), 2);

	st.skip();
	ASSERT_EQ(st.row(), 3);
	ASSERT_EQ(st.column(), 1);

	auto quot1 = st.read(parse::quotation);
	ASSERT_EQ(st.row(), 3);
	ASSERT_EQ(quot1, "\'this is a quotation.\'");
	st.skip();

	auto quot2 = st.read(parse::quotation);
	ASSERT_EQ(st.row(), 4);
	ASSERT_EQ(quot2, "\"this is a quotation, too.\"");

	st.skip();
	ASSERT_EQ(st.row(), 5);
	ASSERT_EQ(st.column(), 1);

	auto dummy = st.read(parse::word("pppp"));
	auto prob = st.read(parse::word("problem"));
	ASSERT_EQ(st.column(), 8);
	ASSERT_EQ(dummy, "");
	ASSERT_EQ(prob, "problem");
}


TEST(ParserTest, Parser)
{
	string_t str =
		"problem name_of_problem\n"
		"{ observe { man(X1) ^ not man(X2) ^ !man(X3) ^ not !man(X4) ^ eat(E,X1,Z) }\n"
		"  require { apple(Z) }\n"
		"  choice{apple(X)}\n"
		"  choice { apple(Y) }\n"
		" }";
	parse::parser_t ps(new std::istringstream(str));

	ps.read();

	ASSERT_TRUE(static_cast<bool>(ps.prob()));

	const problem_t *prob = ps.prob().get();

	const conjunction_t &obs = prob->observation();
	ASSERT_EQ(obs.size(), 5);
	EXPECT_EQ(obs.at(0).string(), "man(X1)");
	EXPECT_EQ(obs.at(1).string(), "not man(X2)");
	EXPECT_EQ(obs.at(2).string(), "!man(X3)");
	EXPECT_EQ(obs.at(3).string(), "not !man(X4)");
	EXPECT_EQ(obs.at(4).string(), "eat(E, X1, Z)");

	const conjunction_t &req = prob->requirement();
	ASSERT_EQ(req.size(), 1);
	EXPECT_EQ(req.at(0).string(), "apple(Z)");
	
	const auto &chs = prob->choices();
	ASSERT_EQ(chs.size(), 2);
	ASSERT_EQ(chs.at(0).size(), 1);
	EXPECT_EQ(chs.at(0).at(0).string(), "apple(X)");
	ASSERT_EQ(chs.at(1).size(), 1);
	EXPECT_EQ(chs.at(1).at(0).string(), "apple(Y)");
}
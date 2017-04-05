#pragma once

#include <gtest/gtest.h>
#include <sstream>
#include <cstring>

#include "../main/parse.h"

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


TEST(ParserTest, InputParser)
{
	string_t str =
		"problem name_of_problem\n"
		"{ observe { man(X1) ^ not man(X2) ^ !man(X3) ^ not !man(X4) ^ eat(E,X1,Z) }\n"
		"  require { apple(Z) }\n"
		"  choice{apple(X)}\n"
		"  choice { apple(Y) }\n"
		" }";
	parse::input_parser_t ps(new std::istringstream(str));

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


TEST(ParserTest, ArgvParser)
{
	std::vector<char*> argv(8, nullptr);
	for (auto &p : argv) p = new char[32];

	std::strcpy(argv[0], "dav");
	std::strcpy(argv[1], "infer");
	std::strcpy(argv[2], "-k");
	std::strcpy(argv[3], "tmp/kb");
	std::strcpy(argv[4], "--long-option");
	std::strcpy(argv[5], "--parallel=8");
	std::strcpy(argv[6], "input_a.txt");
	std::strcpy(argv[7], "input_b.txt");

	parse::argv_parser_t p(argv.size(), &argv[0]);

	EXPECT_EQ(p.mode(), "infer");

	ASSERT_EQ(p.opts().size(), 3);
	EXPECT_EQ(p.opts().at(0).first, "-k");
	EXPECT_EQ(p.opts().at(0).second, "tmp/kb");
	EXPECT_EQ(p.opts().at(1).first, "--long-option");
	EXPECT_EQ(p.opts().at(1).second, "");
	EXPECT_EQ(p.opts().at(2).first, "--parallel");
	EXPECT_EQ(p.opts().at(2).second, "8");

	ASSERT_EQ(p.inputs().size(), 2);
	EXPECT_EQ(p.inputs().at(0), "input_a.txt");
	EXPECT_EQ(p.inputs().at(1), "input_b.txt");

	for (auto *p : argv) delete[] p;
}
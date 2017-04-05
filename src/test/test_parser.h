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
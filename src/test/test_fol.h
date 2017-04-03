#pragma once

#include "../main/fol.h"

using namespace dav;

TEST(FOLTest, Predicate)
{
	predicate_library_t::initialize();

	predicate_t p1("eat", 3);
	predicate_t p2("kill/2");
	predicate_t p3(EQ_PREDICATE_ID);
	predicate_t p4(INVALID_PREDICATE_ID);

	EXPECT_EQ(p1.string(), "eat/3");
	EXPECT_EQ(p1.predicate(), "eat");
	EXPECT_EQ(p1.arity(), 3);
	EXPECT_TRUE(p1.good());

	EXPECT_EQ(p2.string(), "kill/2");
	EXPECT_EQ(p2.predicate(), "kill");
	EXPECT_EQ(p2.arity(), 2);
	EXPECT_TRUE(p2.good());

	EXPECT_EQ(p3.string(), "=/2");
	EXPECT_EQ(p3.predicate(), "=");
	EXPECT_EQ(p3.arity(), 2);
	EXPECT_TRUE(p3.good());

	EXPECT_FALSE(p4.good());
}


TEST(FOLTest, Atom)
{
	predicate_library_t::instance()->add(predicate_t("eat/3"));

	atom_t eat1("eat", { "e", "x", "y" }, false, false);
	atom_t eat2("eat", { "e", "x", "y" }, true, false);
	atom_t eat3("eat", { "e", "x", "y" }, false, true);
	atom_t eat4("eat", { "e", "x", "y" }, true, true);

	EXPECT_EQ(eat1.predicate().predicate(), "eat");
	EXPECT_EQ(eat2.predicate().predicate(), "eat");
	EXPECT_EQ(eat3.predicate().predicate(), "eat");
	EXPECT_EQ(eat4.predicate().predicate(), "eat");

	EXPECT_EQ(eat1.predicate().arity(), 3);
	EXPECT_EQ(eat2.predicate().arity(), 3);
	EXPECT_EQ(eat3.predicate().arity(), 3);
	EXPECT_EQ(eat4.predicate().arity(), 3);

	EXPECT_TRUE(eat1.truth());
	EXPECT_FALSE(eat1.naf());
	EXPECT_FALSE(eat1.neg());

	EXPECT_FALSE(eat2.truth());
	EXPECT_FALSE(eat2.naf());
	EXPECT_TRUE(eat2.neg());

	EXPECT_FALSE(eat3.truth());
	EXPECT_TRUE(eat3.naf());
	EXPECT_FALSE(eat3.neg());

	EXPECT_FALSE(eat4.truth());
	EXPECT_TRUE(eat4.naf());
	EXPECT_TRUE(eat4.neg());

	EXPECT_EQ(eat1.string(), "eat(e, x, y)");
	EXPECT_EQ(eat2.string(), "!eat(e, x, y)");
	EXPECT_EQ(eat3.string(), "not eat(e, x, y)");
	EXPECT_EQ(eat4.string(), "not !eat(e, x, y)");

	term_t x("x"), y("y");
	EXPECT_TRUE(atom_t::equal(x, y) == atom_t::equal(y, x));
	EXPECT_TRUE(atom_t::not_equal(x, y) == atom_t::not_equal(y, x));

	char line[256];
	binary_writer_t wr(line, 256);
	binary_reader_t rd(line, 256);

	wr.write<atom_t>(eat1);
	atom_t eat5(rd);
	EXPECT_TRUE(eat1 == eat5);
	EXPECT_TRUE(wr.size() == rd.size());
}
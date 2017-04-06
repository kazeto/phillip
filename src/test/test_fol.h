#pragma once

#include <gtest/gtest.h>

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
	predicate_library_t::instance()->add(predicate_t("apple/1"));
	predicate_library_t::instance()->add(predicate_t("man/1"));

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


TEST(FOLTest, Conjunction)
{
	conjunction_t conj1;
	conj1.push_back(atom_t("eat", { "e", "x", "y" }, false, false));
	conj1.push_back(atom_t("man", { "x" }, false, false));
	conj1.push_back(atom_t("apple", { "y" }, false, false));
	conj1.param() = "this_is_parameter";

	EXPECT_EQ(conj1.string(), "{eat(e, x, y) ^ man(x) ^ apple(y)}");

	char buf[1024];
	binary_writer_t wr(buf, 1024);
	binary_reader_t rd(buf, 1024);

	wr.write<conjunction_t>(conj1);
	conjunction_t conj2(rd);

	EXPECT_TRUE(conj1 == conj2);

	conjunction_t::feature_t feat1 = conj1.feature();
	wr.reset();
	wr.write<conjunction_t::feature_t>(feat1);
	rd.reset();
	conjunction_t::feature_t feat2(rd);

	EXPECT_TRUE(feat1 == feat2);
}


TEST(FOLTest, Rule)
{
	rule_t r1;
	r1.name() = "this_is:rule_name";
	r1.lhs().push_back(atom_t("apple", { "x" }, false, false));
	r1.rhs().push_back(atom_t("eat", { "e", "y", "x" }, false, false));
	r1.rid() = 1;

	char buf[1024];
	binary_reader_t rd(buf, 1024);
	binary_writer_t wr(buf, 1024);

	wr.write<rule_t>(r1);
	rule_t r2(rd);

	EXPECT_TRUE(rd.size() == wr.size());
	EXPECT_EQ(r2.name(), "this_is:rule_name");
	EXPECT_EQ(r2.classname(), "this_is");
	EXPECT_TRUE(r1.lhs() == r2.lhs());
	EXPECT_TRUE(r1.rhs() == r2.rhs());
}
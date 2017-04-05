#pragma once

#include <gtest/gtest.h>

#include "../main/kb.h"

using namespace dav;


TEST(KBTest, ConjunctionLibrary)
{
	kb::conjunction_library_t lib("tmp.cdb");

	rule_t r;
	r.name() = "this_is_rule_name";
	r.lhs().push_back(atom_t("apple", { "x" }, false, false));
	r.rhs().push_back(atom_t("eat", { "e", "y", "x" }, false, false));
	r.rhs().push_back(atom_t("man", { "y" }, false, false));

	lib.prepare_compile();
	lib.insert(r);
	lib.prepare_query();

	predicate_id_t pid1 = plib()->pred2id("apple/1");
	predicate_id_t pid2 = plib()->pred2id("eat/3");
	predicate_id_t pid3 = plib()->pred2id("man/1");

	auto e1 = lib.get(pid1);
	auto e2 = lib.get(pid2);
	auto e3 = lib.get(pid3);

	ASSERT_EQ(e1.size(), 1);
	ASSERT_EQ(e2.size(), 1);
	ASSERT_EQ(e3.size(), 1);

	EXPECT_FALSE(e1.front().is_backward);
	EXPECT_TRUE(e2.front().is_backward);
	EXPECT_TRUE(e3.front().is_backward);
	EXPECT_TRUE(e2.front().feature == e3.front().feature);
}


TEST(KBTest, FeatureToRulesCDB)
{
	kb::feature_to_rules_cdb_t f2r("tmp.cdb");

	rule_t r;
	r.name() = "this_is_rule_name";
	r.lhs().push_back(atom_t("apple", { "x" }, false, false));
	r.rhs().push_back(atom_t("eat", { "e", "y", "x" }, false, false));
	r.rhs().push_back(atom_t("man", { "y" }, false, false));
	r.rid() = 2;

	f2r.prepare_compile();
	f2r.insert(r);
	f2r.prepare_query();

	auto r1 = f2r.gets(r.lhs().feature(), false);
	EXPECT_EQ(r1.size(), 1);
	EXPECT_EQ(r1.front(), 2);

	auto r2 = f2r.gets(r.lhs().feature(), true);
	EXPECT_EQ(r2.size(), 0);

	auto r3 = f2r.gets(r.rhs().feature(), false);
	EXPECT_EQ(r3.size(), 0);

	auto r4 = f2r.gets(r.rhs().feature(), true);
	EXPECT_EQ(r4.size(), 1);
	EXPECT_EQ(r4.front(), 2);
}


TEST(KBTest, RuleLibrary)
{
	kb::rule_library_t lib("tmp.cdb");
	rule_t r;
	r.name() = "this_is_rule_name";
	r.lhs().push_back(atom_t("apple", { "x" }, false, false));
	r.rhs().push_back(atom_t("eat", { "e", "y", "x" }, false, false));
	r.rhs().push_back(atom_t("man", { "y" }, false, false));

	lib.prepare_compile();
	ASSERT_TRUE(lib.is_writable());
	ASSERT_FALSE(lib.is_readable());

	rule_id_t rid = lib.add(r);

	lib.prepare_query();
	ASSERT_FALSE(lib.is_writable());
	ASSERT_TRUE(lib.is_readable());

	EXPECT_EQ(lib.size(), 1);

	rule_t r2 = lib.get(rid);
	EXPECT_EQ(r2.name(), "this_is_rule_name");
	EXPECT_TRUE(r.lhs() == r2.lhs());
	EXPECT_TRUE(r.rhs() == r2.rhs());
}
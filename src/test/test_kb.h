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
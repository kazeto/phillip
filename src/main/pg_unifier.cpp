#include "./pg.h"

namespace dav
{

namespace pg
{


unifier_t::unifier_t(const atom_t *x, const atom_t *y)
	: std::pair<const atom_t*, const atom_t*>(x, y), m_unifiable(true)
{
	init();
}


void unifier_t::init()
{
	m_unifiable = true;
	if (first->predicate() == second->predicate())
	{
		for (term_idx_t i = 0; i < first->arity(); ++i)
		{
			if (first->term(i).is_unifiable_with(second->term(i)))
				m_map[first->term(i)] = second->term(i);
			else
			{
				m_unifiable = false;
				m_map.clear();
				break;
			}
		}
	}
	else
		m_unifiable = false;
}


std::list<atom_t> unifier_t::products() const
{
	std::list<atom_t> out;

	for (const auto &p : map())
		out.push_back(atom_t::equal(p.first, p.second));

	return out;
}


string_t unifier_t::string() const
{
	string_t exp1 = "unify(" + first->string() + ", " + second->string() + ")";

	std::string exp2;
	for (const auto &eq : products())
	{
		if (exp2.empty())
			exp2 += " ^ ";
		exp2 += eq.string();
	}

	return "{ " + exp1 + " => " + exp2 + " }";
}


}

}
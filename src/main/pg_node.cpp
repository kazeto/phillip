#include "./pg.h"

namespace dav
{

namespace pg
{


node_t::node_t(const atom_t &atom, node_type_e type, node_idx_t idx, depth_t depth)
	: atom_t(atom), m_type(type), m_index(idx),	m_depth(depth)
{}


string_t node_t::string() const
{
	string_t out = format("[%d]", m_index) + atom_t::string();

	if (not param().empty())
		out += ":" + param();

	return out;
}



}

}
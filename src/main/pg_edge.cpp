#include <cassert>

#include "./pg.h"

namespace dav
{

namespace pg
{


edge_t::edge_t()
	: m_type(EDGE_UNSPECIFIED),	m_tail(-1), m_head(-1), m_rid(-1)
{}


edge_t::edge_t(
	edge_type_e type, edge_idx_t idx,
	hypernode_idx_t tail, hypernode_idx_t head, rule_id_t id)
	: m_type(type), m_index(idx), m_tail(tail), m_head(head), m_rid(id)
{
	assert(is_unification() or rid() != INVALID_RULE_ID);
}

}

}
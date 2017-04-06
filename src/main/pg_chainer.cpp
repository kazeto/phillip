#include "./pg.h"

namespace dav
{

namespace pg
{


chainer_t::chainer_t(
	proof_graph_t *pg, rule_id_t rid, is_backward_t b, const std::vector<node_idx_t> &targets)
	: std::tuple<rule_id_t, is_backward_t, std::vector<node_idx_t>>(rid, b, targets),
	m_pg(pg)
{}


std::list<atom_t> chainer_t::products() const
{
	std::list<atom_t> out;

	// TODO

	return out;
}


}

}
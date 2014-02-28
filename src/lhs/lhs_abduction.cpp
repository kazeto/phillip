/* -*- coding:utf-8 -*- */


#include "./lhs_enumerator.h"


namespace phil
{

namespace lhs
{


abductive_enumerator_t::abductive_enumerator_t()
    : m_depth_max(-1)
{
    std::string depth_str = sys()->param("depth");
    if (not depth_str.empty())
    {
        int depth;
        int ret = _sscanf(depth_str.c_str(), "%d", &depth);
        if (ret == 1 and depth > 0)
            m_depth_max = depth;
    }
}



pg::proof_graph_t* abductive_enumerator_t::execute() const
{
    pg::proof_graph_t *out = new pg::proof_graph_t();
    add_observations(out);

    for (int d = 0; (m_depth_max < 0 or d < m_depth_max); ++d)
    {
        const hash_set<pg::node_idx_t>
            *targets = out->search_nodes_with_depth(d);
        
        if (targets == NULL) break;

        for (auto it = targets->begin(); it != targets->end(); ++it)
            chain(*it, out);
    }

    return out;
}


void abductive_enumerator_t::chain(
    pg::node_idx_t idx, pg::proof_graph_t *graph) const
{
    const kb::knowledge_base_t *base = sys()->knowledge_base();
    const pg::node_t &node = graph->node(idx);
    std::list<axiom_id_t> ids =
        base->search_axioms_with_rhs(node.literal().get_predicate_arity());

    for (auto it = ids.begin(); it != ids.end(); ++it)
    {
        lf::axiom_t ax = base->get_axiom(*it);
        std::list< std::vector<pg::node_idx_t> > targets =
            graph->enumerate_targets_of_chain(ax, true, node.depth());

        for (auto v = targets.begin(); v != targets.end(); ++v)
        {
            pg::hypernode_idx_t from = graph->add_hypernode(*v);
            pg::hypernode_idx_t to = graph->backward_chain(from, ax);

            // ---- DEBUG
            if (sys()->verbose() == FULL_VERBOSE)
            {
                const std::vector<pg::node_idx_t>
                    &hn_from = graph->hypernode(from),
                    &hn_to = graph->hypernode(to);
                std::string
                    str_from = join(hn_from.begin(), hn_from.end(), "%d", ","),
                    str_to = join(hn_to.begin(), hn_to.end(), "%d", ",");
                std::string disp =
                    format("Backward-chain: %d:[%s] <= %s <= %d:[%s]",
                           from, str_from.c_str(), ax.name.c_str(),
                           to, str_to.c_str());
                std::cerr << time_stamp() << disp << std::endl;
            }
        }
    }
}


bool abductive_enumerator_t::can_execute(std::list<std::string>*) const
{ return true; }


std::string abductive_enumerator_t::repr() const
{
    return format(
        "AbductiveEnumerator(depth = %d)", m_depth_max);
}


}

}

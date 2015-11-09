/* -*- coding:utf-8 -*- */


#include <ctime>
#include "./lhs_enumerator.h"


namespace phil
{

namespace lhs
{


depth_based_enumerator_t::depth_based_enumerator_t(
    const phillip_main_t *ptr, int max_depth)
    : lhs_enumerator_t(ptr), m_depth_max(max_depth)
{}


lhs_enumerator_t* depth_based_enumerator_t
::duplicate(const phillip_main_t *ptr) const
{
    return new depth_based_enumerator_t(ptr, m_depth_max);
}


pg::proof_graph_t* depth_based_enumerator_t::execute() const
{
    const kb::knowledge_base_t *base(kb::knowledge_base_t::instance());
    pg::proof_graph_t *graph =
        new pg::proof_graph_t(phillip(), phillip()->get_input()->name);
    pg::proof_graph_t::chain_candidate_generator_t gen(graph);

    int max_size = get_max_lhs_size();

    auto begin = std::chrono::system_clock::now();
    add_observations(graph);

    for (int depth = 0; (m_depth_max < 0 or depth < m_depth_max); ++depth)
    {
        const hash_set<pg::node_idx_t>
            *nodes = graph->search_nodes_with_depth(depth);
        if (nodes == NULL) break;

        hash_map<axiom_id_t, std::set<pg::chain_candidate_t>> candidates;

        for (auto n : (*nodes))
        {
            for (gen.init(n); not gen.end(); gen.next())
            {
                for (auto ax : gen.axioms())
                {
                    std::set<pg::chain_candidate_t> &cands = candidates[ax.first];

                    for (auto nodes : gen.targets())
                        cands.insert(pg::chain_candidate_t(
                        nodes, ax.first, not kb::is_backward(ax)));
                }
            }
        }

        for (auto p : candidates)
        {
            const lf::axiom_t &axiom = kb::kb()->get_axiom(p.first);

            for (auto c : p.second)
            {
                pg::hypernode_idx_t to = c.is_forward ?
                    graph->forward_chain(c.nodes, axiom) :
                    graph->backward_chain(c.nodes, axiom);
            }

            if (do_exceed_max_lhs_size(graph, max_size))
                goto ABORT_GEN;

            if (do_time_out(begin))
            {
                graph->timeout(true);
                goto ABORT_GEN;
            }
        }
    }

    ABORT_GEN:

    graph->post_process();
    return graph;
}


bool depth_based_enumerator_t::is_available(std::list<std::string>*) const
{ return true; }


std::string depth_based_enumerator_t::repr() const
{
    return "DepthBasedEnumerator";
}


lhs_enumerator_t* depth_based_enumerator_t::
generator_t::operator()(const phillip_main_t *ph) const
{
    return new lhs::depth_based_enumerator_t(ph, ph->param_int("max_depth"));
}


}

}

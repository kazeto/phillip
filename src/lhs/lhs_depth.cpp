/* -*- coding:utf-8 -*- */


#include <ctime>
#include "./lhs_enumerator.h"


namespace phil
{

namespace lhs
{


depth_based_enumerator_t::depth_based_enumerator_t(
    phillip_main_t *ptr, int max_depth)
    : lhs_enumerator_t(ptr), m_depth_max(max_depth)
{}


lhs_enumerator_t* depth_based_enumerator_t::duplicate(phillip_main_t *ptr) const
{
    return new depth_based_enumerator_t(ptr, m_depth_max);
}


pg::proof_graph_t* depth_based_enumerator_t::execute() const
{
    const kb::knowledge_base_t *base(kb::knowledge_base_t::instance());
    pg::proof_graph_t *graph =
        new pg::proof_graph_t(phillip(), phillip()->get_input()->name);
    pg::proof_graph_t::chain_candidate_generator_t gen(graph);

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

            if (do_time_out(begin))
            {
                graph->timeout(true);
                goto TIMED_OUT;
            }
        }
    }

    TIMED_OUT:

    graph->post_process();
    return graph;
}


std::set<pg::chain_candidate_t> depth_based_enumerator_t::
enumerate_chain_candidates(const pg::proof_graph_t *graph, int depth) const
{
    std::set<pg::chain_candidate_t> out;

    std::set<std::tuple<axiom_id_t, bool> > axioms;
    {
        const hash_set<pg::node_idx_t>
            *nodes = graph->search_nodes_with_depth(depth);

        /* ENUMERATE AXIOMS TO USE */
        if (nodes != NULL)
        for (auto it = nodes->begin(); it != nodes->end(); ++it)
        {
            const pg::node_t &n = graph->node(*it);
            std::string arity = n.literal().get_arity();

            std::list<axiom_id_t> ax_deductive = kb::kb()->search_axioms_with_lhs(arity);
            for (auto ax = ax_deductive.begin(); ax != ax_deductive.end(); ++ax)
                axioms.insert(std::make_tuple(*ax, true));

            std::list<axiom_id_t> ax_abductive = kb::kb()->search_axioms_with_rhs(arity);
            for (auto ax = ax_abductive.begin(); ax != ax_abductive.end(); ++ax)
                axioms.insert(std::make_tuple(*ax, false));
        }
    }

    for (auto it = axioms.begin(); it != axioms.end(); ++it)
    {
        lf::axiom_t axiom = kb::kb()->get_axiom(std::get<0>(*it));
        enumerate_chain_candidates_sub(
            graph, axiom, not std::get<1>(*it), depth, &out);
    }

    return out;
}


void depth_based_enumerator_t::enumerate_chain_candidates_sub(
    const pg::proof_graph_t *graph, const lf::axiom_t &ax, bool is_backward,
    int depth, std::set<pg::chain_candidate_t> *out) const
{
    std::vector<const literal_t*>
        lits = (is_backward ? ax.func.get_rhs() : ax.func.get_lhs());
    std::vector<std::string> arities;

    for (auto it = lits.begin(); it != lits.end(); ++it)
    if (not (*it)->is_equality())
        arities.push_back((*it)->get_arity());

    if (not arities.empty())
    {
        std::list<std::vector<pg::node_idx_t> > targets =
            enumerate_nodes_array_with_arities(graph, arities, depth);
        std::set<pg::chain_candidate_t> _out;

        for (auto nodes : targets)
        if (not do_include_requirement(graph, nodes))
        if (graph->check_nodes_coexistability(nodes.begin(), nodes.end()))
            out->insert(pg::chain_candidate_t(nodes, ax.id, !is_backward));
    }
}


std::list< std::vector<pg::node_idx_t> >
depth_based_enumerator_t::enumerate_nodes_array_with_arities(
const pg::proof_graph_t *graph,
const std::vector<std::string> &arities, int depth) const
{
    std::vector< std::vector<pg::node_idx_t> > candidates;
    std::list< std::vector<pg::node_idx_t> > out;
    bool do_ignore_depth = (depth < 0);

    for (auto it = arities.begin(); it != arities.end(); ++it)
    {
        const hash_set<pg::node_idx_t> *_idx = graph->search_nodes_with_arity(*it);
        if (_idx == NULL) return out;

        std::vector<pg::node_idx_t> _new;
        for (auto n = _idx->begin(); n != _idx->end(); ++n)
        if (do_ignore_depth or graph->node(*n).depth() <= depth)
            _new.push_back(*n);

        if (_new.empty()) return out;

        candidates.push_back(_new);
    }

    std::vector<int> indices(arities.size(), 0);
    bool do_end_loop(false);

    while (not do_end_loop)
    {
        std::vector<pg::node_idx_t> _new;
        bool is_valid(false);

        for (int i = 0; i < candidates.size(); ++i)
        {
            pg::node_idx_t idx = candidates.at(i).at(indices[i]);
            _new.push_back(idx);
            if (do_ignore_depth or graph->node(idx).depth() == depth)
                is_valid = true;
        }

        if (is_valid) out.push_back(_new);

        // INCREMENT
        ++indices[0];
        for (int i = 0; i < candidates.size(); ++i)
        {
            if (indices[i] >= candidates[i].size())
            {
                if (i < indices.size() - 1)
                {
                    indices[i] = 0;
                    ++indices[i + 1];
                }
                else do_end_loop = true;
            }
        }
    }

    return out;
}


bool depth_based_enumerator_t::is_available(std::list<std::string>*) const
{ return true; }


std::string depth_based_enumerator_t::repr() const
{
    return "DepthBasedEnumerator";
}


lhs_enumerator_t* depth_based_enumerator_t::
generator_t::operator()(phillip_main_t *ph) const
{
    return new lhs::depth_based_enumerator_t(ph, ph->param_int("max_depth"));
}


}

}

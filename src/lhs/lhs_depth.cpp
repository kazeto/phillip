/* -*- coding:utf-8 -*- */


#include <ctime>
#include "./lhs_enumerator.h"


namespace phil
{

namespace lhs
{


depth_based_enumerator_t::depth_based_enumerator_t(
    bool do_deduction, bool do_abduction,
    int max_depth, float max_distance, float max_redundancy,
    bool do_disable_reachable_matrix)
    : m_do_deduction(do_deduction), m_do_abduction(do_abduction),
      m_depth_max(max_depth), m_distance_max(max_distance),
      m_redundancy_max(max_redundancy),
      m_do_disable_reachable_matrix(do_disable_reachable_matrix)
{}


pg::proof_graph_t* depth_based_enumerator_t::execute() const
{
    const kb::knowledge_base_t *base(sys()->knowledge_base());
    pg::proof_graph_t *graph(new pg::proof_graph_t(sys()->get_input()->name));
    hash_map<pg::node_idx_t, reachable_map_t> reachability;
    time_t begin, now;

    time(&begin);
    add_observations(graph);

    if (not m_do_disable_reachable_matrix)
        reachability = compute_reachability_of_observations(graph);

    for (int depth = 0; (m_depth_max < 0 or depth < m_depth_max); ++depth)
    {
        std::set<pg::chain_candidate_t> cands =
            enumerate_chain_candidates(graph, depth);
        if (cands.empty()) break;

        hash_map<axiom_id_t, lf::axiom_t> axioms;

        // ENUMERATE AXIOMS USED HERE
        for (auto it = cands.begin(); it != cands.end(); ++it)
        if (axioms.count(it->axiom_id) == 0)
            axioms[it->axiom_id] = base->get_axiom(it->axiom_id);

        // EXECUTE CHAINING
        for (auto it = cands.begin(); it != cands.end(); ++it)
        {
            // CHECK TIME-OUT
            time(&now);
            if (sys()->is_timeout(now - begin))
            {
                graph->timeout(true);
                break;
            }
            
            const lf::axiom_t &axiom = axioms.at(it->axiom_id);
            pg::hypernode_idx_t to(-1);

            if (m_do_disable_reachable_matrix)
            {
                to = it->is_forward ?
                    graph->forward_chain(it->nodes, axiom) :
                    graph->backward_chain(it->nodes, axiom);
            }
            else
            {
                std::vector<reachable_map_t> reachability_new;

                if (not compute_reachability_of_chaining(
                    graph, reachability, it->nodes, axiom,
                    it->is_forward, &reachability_new))
                    continue;

                to = it->is_forward ?
                    graph->forward_chain(it->nodes, axiom) :
                    graph->backward_chain(it->nodes, axiom);

                if (to >= 0)
                {
                    // SET REACHABILITY OF NEW NODES
                    const std::vector<pg::node_idx_t> hn_to = graph->hypernode(to);
                    for (int i = 0; i < hn_to.size(); ++i)
                    {
                        filter_unified_reachability(
                            graph, hn_to[i], &reachability_new[i]);
                        reachability[hn_to.at(i)] = reachability_new[i];
                    }
                }
            }

            // FOR DEBUG
            if (to >= 0 and sys()->verbose() == FULL_VERBOSE)
                print_chain_for_debug(graph, axiom, (*it), to);
        }

        if (graph->is_timeout()) break;
    }

    graph->clean_logs();
    return graph;
}


std::set<pg::chain_candidate_t> depth_based_enumerator_t::
enumerate_chain_candidates(const pg::proof_graph_t *graph, int depth) const
{
    const kb::knowledge_base_t *base = sys()->knowledge_base();
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
            std::string arity = n.literal().get_predicate_arity();

            if (m_do_deduction)
            {
                std::list<axiom_id_t> _axioms =
                    base->search_axioms_with_lhs(arity);
                for (auto ax = _axioms.begin(); ax != _axioms.end(); ++ax)
                    axioms.insert(std::make_tuple(*ax, true));
            }
            if (m_do_abduction)
            {
                std::list<axiom_id_t> _axioms =
                    base->search_axioms_with_rhs(arity);
                for (auto ax = _axioms.begin(); ax != _axioms.end(); ++ax)
                    axioms.insert(std::make_tuple(*ax, false));
            }
        }
    }

    for (auto it = axioms.begin(); it != axioms.end(); ++it)
    {
        lf::axiom_t axiom = base->get_axiom(std::get<0>(*it));
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
        arities.push_back((*it)->get_predicate_arity());

    std::list<std::vector<pg::node_idx_t> > targets =
        enumerate_nodes_array_with_arities(graph, arities, depth);
    std::set<pg::chain_candidate_t> _out;

    for (auto it = targets.begin(); it != targets.end(); ++it)
        _out.insert(pg::chain_candidate_t(*it, ax.id, !is_backward));

#ifndef DISABLE_CUTTING_LHS
    graph->erase_invalid_chain_candidates_with_coexistence(&_out);
#endif

    out->insert(_out.begin(), _out.end());
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


hash_map<pg::node_idx_t, depth_based_enumerator_t::reachable_map_t>
depth_based_enumerator_t::compute_reachability_of_observations(
const pg::proof_graph_t *graph) const
{
    hash_map<pg::node_idx_t, reachable_map_t> out;
    const kb::knowledge_base_t *kb = sys()->knowledge_base();
    hash_set<pg::node_idx_t> obs = graph->enumerate_observations();

    for (auto n1 = obs.begin(); n1 != obs.end(); ++n1)
    for (auto n2 = obs.begin(); n2 != n1; ++n2)
    {
        const pg::node_t &node1 = graph->node(*n1);
        const pg::node_t &node2 = graph->node(*n2);
        float dist = kb->get_distance(
            node1.literal().get_predicate_arity(),
            node2.literal().get_predicate_arity());

        if (dist >= 0 and dist <= m_distance_max)
        {
            reachability_t r = { dist, 0.0f };
            out[*n1][*n2] = r;
            out[*n2][*n1] = r;
        }
    }

    return out;
}


bool depth_based_enumerator_t::compute_reachability_of_chaining(
    const pg::proof_graph_t *graph,
    const hash_map<pg::node_idx_t, reachable_map_t> &reachability,
    const std::vector<pg::node_idx_t> &from,
    const lf::axiom_t &axiom, bool is_forward,
    std::vector<reachable_map_t> *out) const
{
    const kb::knowledge_base_t *base = sys()->knowledge_base();
    hash_set<pg::node_idx_t> evidences;

    /* CREATE evidences */
    for (auto it = from.begin(); it != from.end(); ++it)
    {
        const pg::node_t &node = graph->node(*it);
        evidences.insert(node.evidences().begin(), node.evidences().end());
    }

    reachable_map_t rcs_from;
    std::vector<const literal_t*>
        literals = axiom.func.branch(is_forward ? 1 : 0).get_all_literals();

    /* CREATE rcs_from, WHICH IS A MAP OF REACHABILITY OF from */
    for (auto it1 = from.begin(); it1 != from.end(); ++it1)
    {
        auto find = reachability.find(*it1);
        if (find == reachability.end()) continue;

        const reachable_map_t &rc = find->second;
        for (auto it2 = rc.begin(); it2 != rc.end(); ++it2)
        {
            /* IF TARGET IS INCLUDED IN evidences, EXCLUDES IT. */
            if (evidences.count(it2->first) > 0) continue;

            auto old = rcs_from.find(it2->first);

            if (old == rcs_from.end())
                rcs_from[it2->first] = it2->second;
            else if (it2->second.distance < old->second.distance)
                rcs_from[it2->first] = it2->second;
        }
    }

    /* IF THE REACHABILITY OF from IS EMPTY,
     * THIS CHAINING IS NOT NEEDED TO BE PERFORM. */
    if (rcs_from.empty()) return false;

    out->assign(literals.size(), reachable_map_t());
    float base_distance = base->get_distance(axiom);
    bool can_reach_somewhere(false);

    for (auto it = rcs_from.begin(); it != rcs_from.end(); ++it)
    {
        std::string arity =
            graph->node(it->first).literal().get_predicate_arity();

        for (int i = 0; i < literals.size(); ++i)
        {
            std::string arity2 = literals.at(i)->get_predicate_arity();
            float distance = base->get_distance(arity, arity2);
            float redundancy =
                it->second.redundancy +
                base_distance - (it->second.distance - distance);

            if (distance >= 0.0f
                and distance <= m_distance_max
                and (m_redundancy_max < 0.0 or redundancy <= m_redundancy_max))
            {
                reachability_t rc = { distance, redundancy };
                (*out)[i][it->first] = rc;
                can_reach_somewhere = true;
            }
        }
    }

    return can_reach_somewhere;
}


void depth_based_enumerator_t::filter_unified_reachability(
    const pg::proof_graph_t *graph, pg::node_idx_t target,
    reachable_map_t *out) const
{
    const hash_set<pg::node_idx_t> *nodes =
        graph->search_nodes_with_arity(
        graph->node(target).literal().get_predicate_arity());
    hash_set<pg::node_idx_t> evidences;

    for (auto it = nodes->begin(); it != nodes->end(); ++it)
    if (target != *it)
    {
        const pg::node_t n = graph->node(*it);
        evidences.insert(*it);
        evidences.insert(n.evidences().begin(), n.evidences().end());
    }

    for (auto it = out->begin(); it != out->end();)
    {
        if (evidences.count(it->first) > 0)
            it = out->erase(it);
        else
            ++it;
    }
}


void depth_based_enumerator_t::print_chain_for_debug(
    const pg::proof_graph_t *graph, const lf::axiom_t &axiom,
    const pg::chain_candidate_t &cand, pg::hypernode_idx_t to) const
{
    pg::hypernode_idx_t from =
        graph->find_hypernode_with_ordered_nodes(cand.nodes);
    const std::vector<pg::node_idx_t>
        &hn_from(cand.nodes), &hn_to(graph->hypernode(to));
    std::string
        str_from(join(hn_from.begin(), hn_from.end(), "%d", ",")),
        str_to(join(hn_to.begin(), hn_to.end(), "%d", ","));
    std::string disp(cand.is_forward ? "ForwardChain: " : "BackwardChain: ");

    disp += format("%d:[%s] <= %s <= %d:[%s]",
        from, str_from.c_str(), axiom.name.c_str(), to, str_to.c_str());

    std::cerr << time_stamp() << disp << std::endl;
}


bool depth_based_enumerator_t::is_available(std::list<std::string>*) const
{ return true; }


std::string depth_based_enumerator_t::repr() const
{
    std::string name = m_do_deduction ?
        (m_do_abduction ? "BasicEnumerator" : "BasicDeductiveEnumerator") :
        (m_do_abduction ? "BasicAbductiveEnumerator" : "NullEnumerator");
    return name + format("(depth = %d)", m_depth_max);
}


}

}
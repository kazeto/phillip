/* -*- coding:utf-8 -*- */


#include <ctime>
#include "./lhs_enumerator.h"


namespace phil
{

namespace lhs
{


void a_star_based_enumerator_t::
reachability_manager_t::erase(pg::node_idx_t from, pg::node_idx_t to)
{
    for (auto it = m_list.begin(); it != m_list.end(); ++it)
    {
        if (it->node_from == from and it->node_to == to)
        {
            m_map[from].erase(to);
            m_list.erase(it);
            break;
        }
    }
}


void a_star_based_enumerator_t::reachability_manager_t::
erase(hash_set<pg::node_idx_t> from_set, pg::node_idx_t target)
{
    hash_set<pg::node_idx_t> to_set;
    auto found = m_map.find(target);

    if (found != m_map.end())
    for (auto it = found->second.begin(); it != found->second.end(); ++it)
        to_set.insert(it->first);

    if (not to_set.empty())
    for (auto it = m_list.begin(); it != m_list.end();)
    {
        if (from_set.count(it->node_from) > 0 and to_set.count(it->node_to) > 0)
        {
            m_map[it->node_from].erase(it->node_to);
            it = m_list.erase(it);
        }
        else ++it;
    }
}


a_star_based_enumerator_t::a_star_based_enumerator_t(
    bool do_deduction, bool do_abduction, float max_dist)
    : m_do_deduction(do_deduction), m_do_abduction(do_abduction),
      m_max_distance(max_dist)
{}


pg::proof_graph_t* a_star_based_enumerator_t::execute() const
{
    const kb::knowledge_base_t *base(sys()->knowledge_base());
    pg::proof_graph_t *graph(new pg::proof_graph_t(sys()->get_input()->name));
    ilp_converter_t::enumeration_stopper_t *stopper =
        sys()->ilp_convertor()->enumeration_stopper();
    time_t begin, now;

    time(&begin);
    add_observations(graph);

    reachability_manager_t rm;
    initialize_reachability(graph, &rm);

    while (not rm.empty())
    {
        reachability_t r_target = rm.top();
        hash_set<pg::node_idx_t> from_set;
        std::set<pg::chain_candidate_t> cands;

        from_set.insert(r_target.node_from);
        enumerate_chain_candidates(graph, r_target.node_from, &cands);

        for (auto cand = cands.begin(); cand != cands.end(); ++cand)
        {
            // CHECK TIME-OUT
            time(&now);
            if (sys()->is_timeout(now - begin))
            {
                graph->timeout(true);
                break;
            }

            lf::axiom_t axiom = base->get_axiom(cand->axiom_id);
            std::vector<std::list<reachability_t> > rm_new;

            if (not compute_reachability_of_chaining(
                graph, rm, cand->nodes, axiom, cand->is_forward, &rm_new))
                continue;

            pg::hypernode_idx_t to = cand->is_forward ?
                graph->forward_chain(cand->nodes, axiom) :
                graph->backward_chain(cand->nodes, axiom);
            if (to < 0) continue;

            std::list<reachability_t> satisfied;
            const std::vector<pg::node_idx_t> hn_to = graph->hypernode(to);

            // SET REACHABILITY OF NEW NODES
            for (int i = 0; i < hn_to.size(); ++i)
            {
                for (auto it = rm_new[i].begin(); it != rm_new[i].end(); ++it)
                    it->node_from = hn_to[i];

                erase_satisfied_reachability(
                    graph, hn_to[i], &rm_new[i], &satisfied);
                rm.insert(rm_new[i].begin(), rm_new[i].end());
            }
            from_set.insert(cand->nodes.begin(), cand->nodes.end());

            // FOR DEBUG
            if (sys()->verbose() == FULL_VERBOSE)
                print_chain_for_debug(graph, axiom, (*cand), to);
        }

        rm.erase(from_set, r_target.node_from);

        if (graph->is_timeout()) break;
        if ((*stopper)(graph)) break;
    }

    graph->post_process();
    return graph;
}


void a_star_based_enumerator_t::enumerate_chain_candidates(
    const pg::proof_graph_t *graph, pg::node_idx_t i,
    std::set<pg::chain_candidate_t> *out) const
{
    const kb::knowledge_base_t *base = sys()->knowledge_base();

    std::set<std::tuple<axiom_id_t, bool> > axioms;
    {
        const pg::node_t &n = graph->node(i);
        std::string arity = n.literal().get_predicate_arity();

        if (m_do_deduction)
        {
            std::list<axiom_id_t> found(base->search_axioms_with_lhs(arity));
            for (auto ax = found.begin(); ax != found.end(); ++ax)
                axioms.insert(std::make_tuple(*ax, true));
        }

        if (m_do_abduction)
        {
            std::list<axiom_id_t> found(base->search_axioms_with_rhs(arity));
            for (auto ax = found.begin(); ax != found.end(); ++ax)
                axioms.insert(std::make_tuple(*ax, false));
        }
    }

    for (auto it = axioms.begin(); it != axioms.end(); ++it)
    {
        lf::axiom_t axiom = base->get_axiom(std::get<0>(*it));
        bool is_forward(std::get<1>(*it));

        if ((is_forward and not m_do_deduction) or
            (not is_forward and not m_do_abduction))
            continue;

        enumerate_chain_candidates_sub(graph, axiom, !is_forward, i, out);
    }
}


void a_star_based_enumerator_t::enumerate_chain_candidates_sub(
    const pg::proof_graph_t *graph, const lf::axiom_t &ax, bool is_backward,
    pg::node_idx_t target, std::set<pg::chain_candidate_t> *out) const
{
    std::vector<const literal_t*>
        lits = (is_backward ? ax.func.get_rhs() : ax.func.get_lhs());
    std::vector<std::string> arities;

    for (auto it = lits.begin(); it != lits.end(); ++it)
        arities.push_back((*it)->get_predicate_arity());

    std::list<std::vector<pg::node_idx_t> > targets =
        enumerate_nodes_array_with_arities(graph, arities, target);
    std::set<pg::chain_candidate_t> _out;

    for (auto it = targets.begin(); it != targets.end(); ++it)
        _out.insert(pg::chain_candidate_t(*it, ax.id, !is_backward));

#ifndef DISABLE_CUTTING_LHS
    graph->erase_invalid_chain_candidates_with_coexistence(&_out);
#endif

    out->insert(_out.begin(), _out.end());
}



std::list< std::vector<pg::node_idx_t> >
a_star_based_enumerator_t::enumerate_nodes_array_with_arities(
const pg::proof_graph_t *graph,
const std::vector<std::string> &arities, pg::node_idx_t target) const
{
    std::vector< std::vector<pg::node_idx_t> > candidates;
    std::list< std::vector<pg::node_idx_t> > out;
    bool do_ignore_target = (target < 0);

    for (auto it = arities.begin(); it != arities.end(); ++it)
    {
        const hash_set<pg::node_idx_t> *_idx = graph->search_nodes_with_arity(*it);
        if (_idx == NULL) return out;
        candidates.push_back(
            std::vector<pg::node_idx_t>(_idx->begin(), _idx->end()));
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
            if (do_ignore_target or idx == target)
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


void a_star_based_enumerator_t::initialize_reachability(
const pg::proof_graph_t *graph, reachability_manager_t *out) const
{
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

        if (check_permissibility_of(dist))
        {
            out->add(reachability_t(*n1, *n2, 0.0f, dist));
            out->add(reachability_t(*n2, *n1, 0.0f, dist));
        }
    }
}


bool a_star_based_enumerator_t::compute_reachability_of_chaining(
    const pg::proof_graph_t *graph, const reachability_manager_t &rm,
    const std::vector<pg::node_idx_t> &from, const lf::axiom_t &axiom,
    bool is_forward, std::vector<std::list<reachability_t> > *out) const
{
    const kb::knowledge_base_t *base = sys()->knowledge_base();
    hash_set<pg::node_idx_t> evidences;

    /* CREATE evidences */
    for (auto it = from.begin(); it != from.end(); ++it)
    {
        const pg::node_t &node = graph->node(*it);
        evidences.insert(node.evidences().begin(), node.evidences().end());
    }

    hash_map<pg::node_idx_t, reachability_t> rcs_from;
    std::vector<const literal_t*>
        literals = axiom.func.branch(is_forward ? 1 : 0).get_all_literals();

    /* CREATE rcs_from, WHICH IS A MAP OF REACHABILITY OF from */
    for (auto it1 = from.begin(); it1 != from.end(); ++it1)
    {
        auto find = rm.map().find(*it1);
        if (find == rm.map().end()) continue;

        const hash_map<pg::node_idx_t, const reachability_t*> &rc = find->second;
        for (auto it2 = rc.begin(); it2 != rc.end(); ++it2)
        {
            /* IF TARGET IS INCLUDED IN evidences, EXCLUDES IT. */
            if (evidences.count(it2->first) > 0) continue;

            auto old = rcs_from.find(it2->first);
            const reachability_t &_r = (*it2->second);

            if (old == rcs_from.end())
                rcs_from[it2->first] = _r;
            else if (_r.distance() < old->second.distance())
                rcs_from[it2->first] = _r;
        }
    }

    /* IF THE REACHABILITY OF from IS EMPTY,
     * THIS CHAINING IS NOT NEEDED TO BE PERFORM. */
    if (rcs_from.empty()) return false;

    float d0 = base->get_distance(axiom);
    bool can_reach_somewhere(false);

    out->assign(literals.size(), std::list<reachability_t>());
    for (auto it = rcs_from.begin(); it != rcs_from.end(); ++it)
    {
        const reachability_t &r_from = it->second;
        std::string arity =
            graph->node(it->first).literal().get_predicate_arity();

        for (int i = 0; i < literals.size(); ++i)
        {
            std::string arity2 = literals.at(i)->get_predicate_arity();
            float dist_to = base->get_distance(arity, arity2);
            float dist_from = r_from.dist_from + d0;
            if (dist_to < 0.0f) continue;

            reachability_t _r(-1, it->first, dist_from, dist_to);
            if (check_permissibility_of(_r))
            {
                (*out)[i].push_back(_r);
                can_reach_somewhere = true;
            }
        }
    }

    return can_reach_somewhere;
}


void a_star_based_enumerator_t::erase_satisfied_reachability(
    const pg::proof_graph_t *graph, pg::node_idx_t idx,
    std::list<reachability_t> *target,
    std::list<reachability_t> *erased) const
{
    if (graph->node(idx).is_equality_node() or
        graph->node(idx).is_non_equality_node())
        return;

    const hash_set<pg::node_idx_t> *nodes =
        graph->search_nodes_with_arity(graph->node(idx).arity());
    hash_set<pg::node_idx_t> ev;

    // CREATE ev, WHICH IS EVIDENCES OF
    // NODES WHOSE ARITY IS SAME AS NODE(idx)'S.
    for (auto it = nodes->begin(); it != nodes->end(); ++it)
    if (idx != *it)
    {
        const pg::node_t n = graph->node(*it);
        ev.insert(*it);
        ev.insert(n.evidences().begin(), n.evidences().end());
    }

    for (auto it = target->begin(); it != target->end();)
    {
        if (ev.count(it->node_to) > 0)
        {
            erased->push_back(*it);
            it = target->erase(it);
        }
        else
            ++it;
    }
}


void a_star_based_enumerator_t::print_chain_for_debug(
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


bool a_star_based_enumerator_t::is_available(std::list<std::string>*) const
{ return true; }


std::string a_star_based_enumerator_t::repr() const
{
    std::string name = m_do_deduction ?
        (m_do_abduction ? "A*BasedEnumerator" : "A*BasedDeductiveEnumerator") :
        (m_do_abduction ? "A*BasedAbductiveEnumerator" : "NullEnumerator");
    return name;
}



}

}

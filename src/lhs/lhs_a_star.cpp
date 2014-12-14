/* -*- coding:utf-8 -*- */


#include <ctime>
#include "./lhs_enumerator.h"


namespace phil
{

namespace lhs
{


a_star_based_enumerator_t::a_star_based_enumerator_t(
    phillip_main_t *ptr, float max_dist, int max_depth)
    : lhs_enumerator_t(ptr),
      m_max_distance(max_dist), m_max_depth(max_depth)
{}


lhs_enumerator_t* a_star_based_enumerator_t::duplicate(phillip_main_t *ptr) const
{
    return new a_star_based_enumerator_t(ptr, m_max_distance, m_max_depth);
}


pg::proof_graph_t* a_star_based_enumerator_t::execute() const
{
    const kb::knowledge_base_t *base(kb::knowledge_base_t::instance());
    pg::proof_graph_t *graph =
        new pg::proof_graph_t(phillip(), phillip()->get_input()->name);
    ilp_converter_t::enumeration_stopper_t *stopper =
        phillip()->ilp_convertor()->enumeration_stopper();
    time_t begin, now;
    std::map<pg::chain_candidate_t, pg::hypernode_idx_t> considered;
    
    time(&begin);
    add_observations(graph);

    reachability_manager_t rm;
    initialize_reachability(graph, &rm);

    while (not rm.empty())
    {
        const reachability_t cand = rm.top();
        IF_VERBOSE_FULL("Candidates: " + cand.to_string());

        // CHECK TIME-OUT
        time(&now);
        if (phillip()->is_timeout_lhs(now - begin))
        {
            graph->timeout(true);
            break;
        }

        if (considered.count(static_cast<pg::chain_candidate_t>(cand)) == 0)
        {
            lf::axiom_t axiom = base->get_axiom(cand.axiom_id);
            pg::hypernode_idx_t hn_new = cand.is_forward ?
                graph->forward_chain(cand.nodes, axiom) :
                graph->backward_chain(cand.nodes, axiom);
            
            if (hn_new >= 0)
            {
                const std::vector<pg::node_idx_t> nodes_new = graph->hypernode(hn_new);
                hash_map<pg::node_idx_t, float> goal2dist;

                for (auto it_r = rm.begin(); it_r != rm.end(); ++it_r)
                if (static_cast<pg::chain_candidate_t>(cand)
                    == static_cast<pg::chain_candidate_t>(*it_r))
                {
                    float d = it_r->dist_from;
                    auto found = goal2dist.find(it_r->node_to);

                    if (found == goal2dist.end())
                        goal2dist[it_r->node_to] = d;
                    else if (found->second > d)
                        goal2dist[it_r->node_to] = d;
                }

                for (auto g : goal2dist)
                {
                    std::string arity_goal = graph->node(g.first).arity();

                    for (auto n : nodes_new)
                    {
                        if (graph->node(n).arity() == arity_goal)
                        {
                            float dist = g.second + base->get_distance(axiom);
                            for (auto n : nodes_new)
                                add_reachability(
                                graph, cand.node_from, n, cand.node_to, dist, &rm);
                        }
                    }
                }
            }

            considered.insert(
                std::make_pair(static_cast<pg::chain_candidate_t>(cand), hn_new));
        }

        for (auto it = rm.begin(); it != rm.end();)
        {
            if (static_cast<pg::chain_candidate_t>(cand)
                == static_cast<pg::chain_candidate_t>(*it))
                it = rm.erase(it);
            else
                ++it;
        }

        if ((*stopper)(graph)) break;
    }

    graph->post_process();
    return graph;
}


void a_star_based_enumerator_t::enumerate_chain_candidates(
    const pg::proof_graph_t *graph, pg::node_idx_t i,
    std::set<pg::chain_candidate_t> *out) const
{
    auto enumerate = [this](
        const pg::proof_graph_t *graph, const lf::axiom_t &ax, bool is_backward,
        pg::node_idx_t target, std::set<pg::chain_candidate_t> *out)
    {
        auto enumerate_nodes_array_with_arities = [this](
            const pg::proof_graph_t *graph,
            const std::vector<std::string> &arities, pg::node_idx_t target)
        {
            std::vector< std::vector<pg::node_idx_t> > candidates;
            std::list< std::vector<pg::node_idx_t> > out;
            std::string arity_target = graph->node(target).arity();

            for (auto it_arity = arities.begin(); it_arity != arities.end(); ++it_arity)
            {
                bool is_target_arity = (*it_arity == arity_target);
                const hash_set<pg::node_idx_t> *_indices =
                    graph->search_nodes_with_arity(*it_arity);

                if (_indices == NULL) return out;

                candidates.push_back(std::vector<pg::node_idx_t>());
                for (auto it_idx = _indices->begin(); it_idx != _indices->end(); ++it_idx)
                if ((not is_target_arity or *it_idx == target) and
                    (m_max_depth < 0 or graph->node(*it_idx).depth() < m_max_depth))
                    candidates.back().push_back(*it_idx);

                if (candidates.back().empty())
                    return out;
            }

            std::vector<int> indices(arities.size(), 0);
            bool do_end_loop(false);

            while (not do_end_loop)
            {
                std::vector<pg::node_idx_t> _new;

                for (int i = 0; i < candidates.size(); ++i)
                {
                    pg::node_idx_t idx = candidates.at(i).at(indices[i]);
                    _new.push_back(idx);
                }

                out.push_back(_new);

                // INCREMENT
                ++indices[0];
                for (int i = 0; i < candidates.size(); ++i)
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

            return out;
        };

        std::vector<const literal_t*>
            lits = (is_backward ? ax.func.get_rhs() : ax.func.get_lhs());
        std::vector<std::string> arities;

        for (auto it = lits.begin(); it != lits.end(); ++it)
        if (not(*it)->is_equality())
            arities.push_back((*it)->get_arity());

        if (not arities.empty())
        {
            std::list<std::vector<pg::node_idx_t> > targets =
                enumerate_nodes_array_with_arities(graph, arities, target);
            std::set<pg::chain_candidate_t> _out;

            for (auto it = targets.begin(); it != targets.end(); ++it)
            if (not do_include_requirement(graph, *it))
                _out.insert(pg::chain_candidate_t(*it, ax.id, !is_backward));

            graph->erase_invalid_chain_candidates_with_coexistence(&_out);
            out->insert(_out.begin(), _out.end());
        }
    };

    const kb::knowledge_base_t *base = kb::knowledge_base_t::instance();
    const pg::node_t &n = graph->node(i);

    if (m_max_depth >= 0 and n.depth() >= m_max_depth) return;

    std::set<std::tuple<axiom_id_t, bool> > axioms;
    {
        std::string arity = n.literal().get_arity();

        std::list<axiom_id_t> ax_deductive(base->search_axioms_with_lhs(arity));
        for (auto ax = ax_deductive.begin(); ax != ax_deductive.end(); ++ax)
            axioms.insert(std::make_tuple(*ax, true));

        std::list<axiom_id_t> ax_abductive(base->search_axioms_with_rhs(arity));
        for (auto ax = ax_abductive.begin(); ax != ax_abductive.end(); ++ax)
            axioms.insert(std::make_tuple(*ax, false));
    }

    for (auto it = axioms.begin(); it != axioms.end(); ++it)
    {
        lf::axiom_t axiom = base->get_axiom(std::get<0>(*it));
        bool is_forward(std::get<1>(*it));

        enumerate(graph, axiom, !is_forward, i, out);
    }
}


void a_star_based_enumerator_t::initialize_reachability(
const pg::proof_graph_t *graph, reachability_manager_t *out) const
{
    const kb::knowledge_base_t *kb = kb::knowledge_base_t::instance();
    hash_set<pg::node_idx_t> obs = graph->enumerate_observations();

    for (auto n1 = obs.begin(); n1 != obs.end(); ++n1)
    for (auto n2 = obs.begin(); n2 != n1; ++n2)
    {
        float dist = kb->get_distance(
            graph->node(*n1).literal().get_arity(),
            graph->node(*n2).literal().get_arity());

        if (check_permissibility_of(dist))
        {
            add_reachability(graph, *n1, *n1, *n2, 0.0f, out);
            add_reachability(graph, *n2, *n2, *n1, 0.0f, out);
        }
    }
}


void a_star_based_enumerator_t::add_reachability(
    const pg::proof_graph_t *graph,
    pg::node_idx_t start, pg::node_idx_t current, pg::node_idx_t goal, float dist,
    reachability_manager_t *out) const
{
    if (not check_permissibility_of(dist)) return;
    
    const kb::knowledge_base_t *kb = kb::knowledge_base_t::instance();
    std::set<pg::chain_candidate_t> cands;
    std::string arity_goal = graph->node(goal).arity();

    enumerate_chain_candidates(graph, current, &cands);

    for (auto c : cands)
    {
        lf::axiom_t ax = kb->get_axiom(c.axiom_id);
        float d_from = dist + kb->get_distance(ax);

        if (check_permissibility_of(d_from))
        {
            float d_to(-1.0f);

            for (auto l : (c.is_forward ? ax.func.get_rhs() : ax.func.get_lhs()))
            {
                float d = kb->get_distance(l->get_arity(), arity_goal);
                if (check_permissibility_of(d))
                if (d_to < 0.0f or d_to > d)
                    d_to = d;
            }

            if (check_permissibility_of(d_to))
            if (check_permissibility_of(d_from + d_to))
                out->push(reachability_t(c, start, goal, d_from, d_to));
        }
    }
}


bool a_star_based_enumerator_t::is_available(std::list<std::string>*) const
{ return true; }


std::string a_star_based_enumerator_t::repr() const
{
    return "A*BasedEnumerator";
}


std::string a_star_based_enumerator_t::reachability_t::to_string() const
{
    std::string from = join(nodes.begin(), nodes.end(), "%d", ", ");
    return format(
        "nodes: {%s}, axiom: %d, reachability: [%d](dist = %f) -> [%d](dist = %f)",
        from.c_str(), axiom_id, node_from, dist_from, node_to, dist_to);
}


void a_star_based_enumerator_t::reachability_manager_t::push(const reachability_t& r)
{
    float d = r.distance();

    for (auto it = begin(); it != end(); ++it)
    if (d <= it->distance())
    {
        insert(it, r);
        return;
    }

    push_back(r);
}


}

}

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
    std::map<pg::chain_candidate_t, pg::hypernode_idx_t> considered;
    
    auto begin = std::chrono::system_clock::now();
    add_observations(graph);

    reachability_manager_t rm;
    initialize_reachability(graph, &rm);

    while (not rm.empty())
    {
        const reachability_t cand = rm.top();
        IF_VERBOSE_FULL("Candidates: " + cand.to_string());

        // CHECK TIME-OUT
        duration_time_t passed = duration_time(begin);
        if (do_time_out(begin))
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
                hash_map<pg::node_idx_t,
                    std::pair<float, hash_set<pg::node_idx_t> > > from2goals;

                // ENUMERATE REACHABLE-NODE AND THEIR PRE-ESTIMATED DISTANCE.
                for (auto rc : rm)
                {
                    if (static_cast<pg::chain_candidate_t>(cand)
                        == static_cast<pg::chain_candidate_t>(rc))
                    {
                        float d = rc.dist_from;
                        auto found = from2goals.find(rc.node_to);

                        if (found == from2goals.end())
                        {
                            hash_set<pg::node_idx_t> set{ rc.node_to };
                            from2goals[rc.node_from] = std::make_pair(d, set);
                        }
                        else
                        {
                            assert(found->second.first == d);
                            found->second.second.insert(rc.node_to);
                        }
                    }
                }

                for (auto p : from2goals)
                {
                    float dist = p.second.first + base->get_distance(axiom);

                    for (auto n : nodes_new)
                    {
                        add_reachability(
                            graph, p.first, n, dist, p.second.second, &rm);
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
    }

    graph->post_process();
    return graph;
}


void a_star_based_enumerator_t::enumerate_chain_candidates(
    const pg::proof_graph_t *graph, pg::node_idx_t pivot,
    std::set<pg::chain_candidate_t> *out) const
{
    auto enumerate_chain_candidates = [this](
        const pg::proof_graph_t *graph, const kb::arity_pattern_t &pat,
        pg::node_idx_t target, std::set<pg::chain_candidate_t> *out)
    {
        auto enumerate_nodes_array_with_arities = [this](
            const pg::proof_graph_t *graph,
            const std::vector<arity_t> &arities, pg::node_idx_t target)
        {
            std::vector< std::vector<pg::node_idx_t> > candidates;
            std::list< std::vector<pg::node_idx_t> > out;
            std::string arity_target = graph->node(target).arity();

            for (auto a : arities)
            {
                bool is_target_arity = (a == arity_target);
                hash_set<pg::node_idx_t> indices;

                graph->enumerate_nodes_softly_unifiable(a, &indices);

                if (indices.empty()) return out;

                candidates.push_back(std::vector<pg::node_idx_t>());
                for (auto _idx : indices)
                if ((not is_target_arity or _idx == target) and
                    (m_max_depth < 0 or graph->node(_idx).depth() < m_max_depth))
                    candidates.back().push_back(_idx);

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

        std::vector<const lf::logical_function_t*> branches;
        std::vector<arity_t> arities;

        for (auto id : kb::arities(pat))
            arities.push_back(kb::kb()->search_arity(id));

        if (not arities.empty())
        {
            std::list<std::vector<pg::node_idx_t> > targets =
                enumerate_nodes_array_with_arities(graph, arities, target);
            std::set<pg::chain_candidate_t> _out;
            std::list<std::pair<axiom_id_t, bool> > axs; // <AXIOM_ID, IS_BACKWARD>

            kb::kb()->search_axioms_with_arity_pattern(pat, &axs);

            for (auto nodes : targets)
            {
                if (not do_include_requirement(graph, nodes))
                if (graph->check_nodes_coexistability(nodes.begin(), nodes.end()))
                for (auto ax : axs)
                    out->insert(pg::chain_candidate_t(nodes, ax.first, !ax.second));
            }
        }
    };

    const kb::knowledge_base_t *base = kb::knowledge_base_t::instance();

    if (m_max_depth >= 0 and
        graph->node(pivot).depth() >= m_max_depth)
        return;

    std::list<kb::arity_pattern_t> queries;
    graph->enumerate_arity_patterns(pivot, &queries);

    for (auto q : queries)
    {
        std::list<std::pair<axiom_id_t, bool> > axs; // <AXIOM_ID, IS_BACKWARD>
        base->search_axioms_with_arity_pattern(q, &axs);
    }
}


void a_star_based_enumerator_t::initialize_reachability(
const pg::proof_graph_t *graph, reachability_manager_t *out) const
{
    const kb::knowledge_base_t *kb = kb::knowledge_base_t::instance();
    hash_set<pg::node_idx_t> obs = graph->observation_indices();

    for (auto n1 = obs.begin(); n1 != obs.end(); ++n1)
    for (auto n2 = obs.begin(); n2 != n1; ++n2)
    {
        float dist = kb->get_distance(
            graph->node(*n1).literal().get_arity(),
            graph->node(*n2).literal().get_arity());

        if (check_permissibility_of(dist))
        {
            add_reachability(graph, *n1, *n1, 0.0f, { *n2 }, out);
            add_reachability(graph, *n2, *n2, 0.0f, { *n1 }, out);
        }
    }
}


void a_star_based_enumerator_t::add_reachability(
    const pg::proof_graph_t *graph,
    pg::node_idx_t start, pg::node_idx_t current, float dist,
    const hash_set<pg::node_idx_t> &goals,
    reachability_manager_t *out) const
{    
    if (not check_permissibility_of(dist)) return;

    hash_set<pg::node_idx_t> goals_filtered;
    {
        arity_t arity_current = graph->node(current).arity();
        for (auto g : goals)
        if (graph->node(g).arity() != arity_current)
            goals_filtered.insert(g);
    }
    if (goals_filtered.empty()) return;

    std::set<pg::chain_candidate_t> cands;
    enumerate_chain_candidates(graph, current, &cands);

    for (auto c : cands)
    {
        lf::axiom_t ax = kb::kb()->get_axiom(c.axiom_id);
        float d_from = dist + kb::kb()->get_distance(ax);

        // NOTE: MOST CANDIDATES CANNOT PASS THIS IF-STATEMENT.
        if (check_permissibility_of(d_from))
        for (auto g : goals_filtered)
        {
            std::string arity_goal = graph->node(g).arity();
            float d_to(-1.0f);

            for (auto l : (c.is_forward ? ax.func.get_rhs() : ax.func.get_lhs()))
            {
                float d = kb::kb()->get_distance(l->get_arity(), arity_goal);
                if (check_permissibility_of(d))
                if (d_to < 0.0f or d_to > d)
                    d_to = d;
            }

            if (check_permissibility_of(d_to))
            if (check_permissibility_of(d_from + d_to))
                out->push(reachability_t(c, start, g, d_from, d_to));
        }
    }
}


bool a_star_based_enumerator_t::is_available(std::list<std::string>*) const
{ return true; }


std::string a_star_based_enumerator_t::repr() const
{
    return "A*BasedEnumerator";
}


lhs_enumerator_t* a_star_based_enumerator_t::
generator_t::operator()(phillip_main_t *ph) const
{
    return new lhs::a_star_based_enumerator_t(
        ph,
        ph->param_float("max_distance"),
        ph->param_int("max_depth"));
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

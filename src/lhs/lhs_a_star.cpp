/* -*- coding:utf-8 -*- */


#include <ctime>
#include "./lhs_enumerator.h"


namespace phil
{

namespace lhs
{


a_star_based_enumerator_t::a_star_based_enumerator_t(
    const phillip_main_t *ptr, float max_dist, int max_depth)
    : lhs_enumerator_t(ptr),
      m_max_distance(max_dist), m_max_depth(max_depth)
{}


pg::proof_graph_t* a_star_based_enumerator_t::execute() const
{
    const kb::knowledge_base_t *base(kb::knowledge_base_t::instance());
    pg::proof_graph_t *graph =
        new pg::proof_graph_t(phillip(), phillip()->get_input()->name);
    std::map<pg::chain_candidate_t, pg::hypernode_idx_t> considered;

    int max_size = get_max_lhs_size();

    auto begin = std::chrono::system_clock::now();
    add_observations(graph);

    reachability_manager_t rm;
    initialize_reachability(graph, &rm);

    while (not rm.empty())
    {
        const reachability_t cand = rm.top();
        IF_VERBOSE_FULL("Candidates: " + cand.to_string());

        // CHECK TIME-OUT
        duration_time_t passed = util::duration_time(begin);
        if (do_time_out(begin))
        {
            graph->timeout(true);
            break;
        }

        // CHECK LHS-SIZE
        if (do_exceed_max_lhs_size(graph, max_size))
            break;

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
                        auto found = from2goals.find(rc.node_from);

                        if (found == from2goals.end())
                        {
                            hash_set<pg::node_idx_t> set{ rc.node_to };
                            from2goals[rc.node_from] = std::make_pair(rc.dist_from, set);
                        }
                        else
                        {
                            assert(found->second.first == rc.dist_from);
                            found->second.second.insert(rc.node_to);
                        }
                    }
                }

                for (auto p : from2goals)
                for (auto n : nodes_new)
                {
                    const pg::node_idx_t &node_from = p.first;
                    const float &dist_from = p.second.first;
                    const hash_set<pg::node_idx_t> &nodes_to = p.second.second;

                    add_reachability(
                        graph, node_from, n, dist_from, nodes_to, &rm);
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
    const hash_set<pg::node_idx_t> &goals, reachability_manager_t *out) const
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

    pg::proof_graph_t::chain_candidate_generator_t gen(graph);
    for (gen.init(current); not gen.end(); gen.next())
    {
        for (auto ax : gen.axioms())
        {
            lf::axiom_t axiom = kb::kb()->get_axiom(ax.first);
            float d_from = dist + kb::kb()->get_distance(axiom);
            
            if (not check_permissibility_of(d_from)) continue;
            
            for (auto g : goals_filtered)
            {
                std::string arity_goal = graph->node(g).arity();

                for (auto tar : gen.targets())
                {
                    auto lits = not kb::is_backward(ax) ?
                        axiom.func.get_rhs() : axiom.func.get_lhs();
                    float d_to(-1.0f);

                    for (auto l : lits)
                    {
                        float d = kb::kb()->get_distance(l->get_arity(), arity_goal);
                        if ((d_to < 0.0f or d_to > d)
                            and check_permissibility_of(d))
                            d_to = d;
                    }

                    if (not check_permissibility_of(d_to)) continue;
                    if (not check_permissibility_of(d_from + d_to)) continue;

                    out->push(reachability_t(
                        pg::chain_candidate_t(tar, ax.first, !ax.second),
                        start, g, d_from, d_to));
                }
            }
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
generator_t::operator()(const phillip_main_t *ph) const
{
    return new lhs::a_star_based_enumerator_t(
        ph,
        ph->param_float("max_distance"),
        ph->param_int("max_depth"));
}


std::string a_star_based_enumerator_t::reachability_t::to_string() const
{
    std::string from = util::join(nodes.begin(), nodes.end(), ", ");
    return util::format(
        "nodes: {%s}, axiom: %d, reachability: [%d](dist = %f) -> [%d](dist = %f)",
        from.c_str(), axiom_id, node_from, dist_from, node_to, dist_to);
}


void a_star_based_enumerator_t::reachability_manager_t::push(const reachability_t& r)
{
    float d1 = r.distance();

    for (auto it = begin(); it != end(); ++it)
    {
        float d2 = it->distance();

        if ((d1 < d2) or (d1 == d2 and r.dist_from > it->dist_from))
        {
            insert(it, r);
            return;
        }
    }

    push_back(r);
}


}

}

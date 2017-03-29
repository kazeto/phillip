/* -*- coding:utf-8 -*- */


#include <ctime>
#include "./lhs_enumerator.h"


namespace phil
{

namespace lhs
{


a_star_based_enumerator_t::a_star_based_enumerator_t(
    const phillip_main_t *ptr, float max_dist, int max_depth,
    bool disable_unification_counts)
    : lhs_enumerator_t(ptr),
      m_max_distance(max_dist), m_max_depth(max_depth),
      m_do_check_unified_path(not disable_unification_counts)
{}


pg::proof_graph_t* a_star_based_enumerator_t::execute() const
{
    const kb::knowledge_base_t *base(kb::knowledge_base_t::instance());
    pg::proof_graph_t *graph =
        new pg::proof_graph_t(phillip(), phillip()->get_input()->name);
    std::map<pg::chain_candidate_t, pg::hypernode_idx_t> considered;

    int max_size = get_max_lhs_size();
    auto begin = std::chrono::system_clock::now();
    std::map<std::pair<pg::node_idx_t, pg::node_idx_t>, int> unified_paths;
    reachability_manager_t rm;
    reachability_list_t &rl = rm[kb::INVALID_PREDICATE_ID];

    add_observations(graph);
    initialize_reachability(graph, &rm);

    while (not rl.empty())
    {
        const reachability_t cand = rl.top();
        IF_VERBOSE_FULL("Candidates: " + cand.to_string());

        // CHECK TIME-OUT
        duration_time_t passed = util::duration_time(begin);
        if (do_time_out(begin))
        {
            graph->timeout(true);
            break;
        }

        // CHECK THE SIZE OF LATENT HYPOTHESES SET
        if (do_exceed_max_lhs_size(graph, max_size))
            break;

        // IF THE CANDIDATE HAS BEEN NOT PROCESSED YET, THEN PROCESS IT
        if (considered.count(static_cast<pg::chain_candidate_t>(cand)) == 0)
        {
            lf::axiom_t axiom = base->axioms.get(cand.axiom_id);
            pg::hypernode_idx_t hn_new = cand.is_forward ?
                graph->forward_chain(cand.nodes, axiom) :
                graph->backward_chain(cand.nodes, axiom);
            
            // IF CHAINING HAD BEEN PERFORMED, POST-PROCESS THE NEW NODES
            if (hn_new >= 0)
            {
                const std::vector<pg::node_idx_t> nodes_new = graph->hypernode(hn_new);
                hash_map<pg::node_idx_t, std::pair<float, hash_set<pg::node_idx_t>>> from2goals;

                // ENUMERATE NODES REACHABLE FROM hn_new AND THEIR PRE-ESTIMATED DISTANCE.
                for (auto rc : rl)
                if (cand == rc)
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

                // ADD REACHABILITIES FROM nodes_new
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

        // REMOVE CANDIDATES SIMILAR TO cand
        for (auto it = rl.begin(); it != rl.end();)
        {
            if ((*it) == cand)
                it = rl.erase(it);
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
            graph->node(*n1).literal().predicate_with_arity(),
            graph->node(*n2).literal().predicate_with_arity());

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

    // ENUMERATE OBSERVATION-NODES WHICH ARE REACHABLE FROM start
    hash_set<pg::node_idx_t> goals_filtered;
    {
        predicate_with_arity_t arity_current = graph->node(current).predicate_with_arity();
        for (const auto &g : goals)
        if (graph->node(g).predicate_with_arity() != arity_current)
            goals_filtered.insert(g);
    }
    if (goals_filtered.empty()) return;

    pg::proof_graph_t::chain_candidate_generator_t gen(graph);

    // ITERATE AXIOMS WHICH IS APPLICABLE TO current.
    for (gen.init(current); not gen.end(); gen.next())
    {
        // CONSTRUCT A MAP FROM A TARGET TO THE CORRESPONDING REACHABILITY-LIST
        std::map<
            const pg::proof_graph_t::chain_candidate_generator_t::target_nodes_t*,
            reachability_list_t*> tar2rch;
        for (const auto &tar : gen.targets())
        {
            kb::predicate_id_t pred = kb::INVALID_PREDICATE_ID;
            for (index_t i = 0; i < tar.size(); ++i)
            if (tar.at(i) == -1)
            {
                pred = gen.predicates().at(i);
                break;
            }
            tar2rch[&tar] = &(*out)[pred];
        }

        // APPLY EACH AXIOM TO TARGETS
        for (auto ax : gen.axioms())
        {
            lf::axiom_t axiom = kb::kb()->axioms.get(ax.first);
            float d_from = dist + kb::kb()->get_distance(axiom);

            if (not check_permissibility_of(d_from)) continue;

            // EACH OBSERVATION-NODE g WHICH IS THE ENDPOINT OF THE PATH
            for (auto g : goals_filtered)
            {
                for (const auto &tar : gen.targets())
                {
                    auto lits = not kb::is_backward(ax) ?
                        axiom.func.get_rhs() : axiom.func.get_lhs();
                    float d_to(-1.0f);

                    for (const auto &l : lits)
                    {
                        kb::predicate_id_t l_id = kb::kb()->predicates.pred2id(l->predicate_with_arity());
                        kb::predicate_id_t g_id = graph->node(g).predicate_id();
                        float d = kb::kb()->get_distance(l_id, g_id);

                        if ((d_to < 0.0f or d_to > d)
                            and check_permissibility_of(d))
                            d_to = d;
                    }

                    if (not check_permissibility_of(d_to)) continue;
                    if (not check_permissibility_of(d_from + d_to)) continue;

                    tar2rch[&tar]->push(reachability_t(
                        pg::chain_candidate_t(tar, ax.first, !ax.second),
                        start, g, d_from, d_to));
                }
            }
        }
    }
}


bool a_star_based_enumerator_t::is_available(std::list<std::string>*) const
{ return true; }


void a_star_based_enumerator_t::write(std::ostream *os) const
{
    (*os)
        << "<generator name=\"a*\" max-distance=\"" << m_max_distance
        << "\" max-depth=\"" << m_max_depth
        << "\"></generator>" << std::endl;
}


lhs_enumerator_t* a_star_based_enumerator_t::
generator_t::operator()(const phillip_main_t *ph) const
{
    return new lhs::a_star_based_enumerator_t(
        ph,
        ph->param_float("max-distance"),
        ph->param_int("max-depth"),
        ph->flag("disable-check-unified-path"));
}


std::string a_star_based_enumerator_t::reachability_t::to_string() const
{
    std::string from = util::join(nodes.begin(), nodes.end(), ", ");
    return util::format(
        "nodes: {%s}, axiom: %d, reachability: [%d](dist = %f) -> [%d](dist = %f)",
        from.c_str(), axiom_id, node_from, dist_from, node_to, dist_to);
}


void a_star_based_enumerator_t::reachability_list_t::push(const reachability_t& r)
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

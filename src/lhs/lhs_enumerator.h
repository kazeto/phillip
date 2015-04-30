#pragma once

#include <set>
#include <tuple>
#include <queue>

#include "../phillip.h"


namespace phil
{

/** A namespace about factories of latent-hypotheses-sets. */
namespace lhs
{


/** A class to create latent-hypotheses-set of abduction.
 *  Creation is performed with following the mannar of A* search. */
class a_star_based_enumerator_t : public lhs_enumerator_t
{
public:
    struct generator_t : public component_generator_t<lhs_enumerator_t>
    {
        virtual lhs_enumerator_t* operator()(phillip_main_t*) const override;
    };

    a_star_based_enumerator_t(
        phillip_main_t *ptr, float max_dist, int max_depth = -1);
    virtual lhs_enumerator_t* duplicate(phillip_main_t *ptr) const;
    virtual pg::proof_graph_t* execute() const;
    virtual bool is_available(std::list<std::string>*) const;
    virtual std::string repr() const;
    virtual bool do_keep_optimality_on_timeout() const override { return true; }

private:
    struct reachability_t : public pg::chain_candidate_t
    {
        inline reachability_t();
        inline reachability_t(
            const pg::chain_candidate_t&,
            pg::node_idx_t, pg::node_idx_t, float, float);
        float distance() const { return dist_from + dist_to; }
        std::string to_string() const;

        pg::node_idx_t node_from; // The start node.
        pg::node_idx_t node_to;   // The goal node.
        float dist_from; // Distance from the start node new nodes.
        float dist_to;   // Distance from new node to the goal node.
    };

    struct reachability_manager_t : public std::list<reachability_t>
    {
        const reachability_t &top() const { return front(); }
        void push(const reachability_t&);
    };

    void initialize_reachability(
        const pg::proof_graph_t*, reachability_manager_t*) const;
    void add_reachability(
        const pg::proof_graph_t*,
        pg::node_idx_t, pg::node_idx_t, float, const hash_set<pg::node_idx_t>&,
        reachability_manager_t*) const;

    void enumerate_chain_candidates(
        const pg::proof_graph_t *graph, pg::node_idx_t i,
        std::set<pg::chain_candidate_t> *out) const;

    inline bool check_permissibility_of(float dist) const;
    inline bool check_permissibility_of(const reachability_t &r) const;

    float m_max_distance;
    int m_max_depth;
};


/** A class to create latent-hypotheses-set of abduction.
 *  Creation is limited with depth. */
class depth_based_enumerator_t : public lhs_enumerator_t
{
public:
    struct generator_t : public component_generator_t<lhs_enumerator_t>
    {
        virtual lhs_enumerator_t* operator()(phillip_main_t*) const override;
    };

    depth_based_enumerator_t(
        phillip_main_t *ptr,
        int max_depth, float max_distance, float max_redundancy,
        bool do_disable_reachable_matrix = false);
    virtual lhs_enumerator_t* duplicate(phillip_main_t *ptr) const;
    virtual pg::proof_graph_t* execute() const;
    virtual bool is_available(std::list<std::string>*) const;
    virtual std::string repr() const;
    virtual bool do_keep_optimality_on_timeout() const override { return true; }

private:
    struct reachability_t { float distance, redundancy; };
    typedef hash_map<pg::node_idx_t, reachability_t > reachable_map_t;

    /** This is a sub-routine of execute.
    *  Creates reachability map for observations. */
    hash_map<pg::node_idx_t, reachable_map_t>
        compute_reachability_of_observations(const pg::proof_graph_t*) const;

    /** This is a sub-routine of execute.
    *  Computes reachability of new nodes
    *  and returns possibility of the chaining. */
    bool compute_reachability_of_chaining(
        const pg::proof_graph_t *graph,
        const hash_map<pg::node_idx_t, reachable_map_t> &reachability,
        const std::vector<pg::node_idx_t> &from,
        const lf::axiom_t &axiom, bool is_forward,
        std::vector<reachable_map_t> *out) const;

    /** This is a sub-routine of execute.
    *  Gets candidates of chaining from nodes containing a node
    *  whose depth is equals to given depth. */
    std::set<pg::chain_candidate_t> enumerate_chain_candidates(
        const pg::proof_graph_t *graph, int depth) const;

    /** Enumerate candidates for chaining.
     *  Following candidates are excluded from output:
     *    - One is not feasible due to exclusiveness of chains.
     *    - One includes a node whose depth exceeds target depth.
     *    - One do not includes a node whose depth equals to target depth.
     *  @param depth       Target dpeth.
     *                     If -1, limitation on depth will be ignored. */
    void enumerate_chain_candidates_sub(
        const pg::proof_graph_t *graph, const lf::axiom_t &ax, bool is_backward,
        int depth, std::set<pg::chain_candidate_t> *out) const;

    /** Returns arrays of nodes whose arities are same as given arities.
    *  @param arities Arrays of arities to search.
    *  @param target  Each vector must include this node. */
    std::list< std::vector<pg::node_idx_t> > enumerate_nodes_array_with_arities(
        const pg::proof_graph_t *graph,
        const std::vector<std::string> &arities, int depth) const;

    /** This is a sub-routine of execute.
    *  Erases satisfied reachabilities from out. */
    void filter_unified_reachability(
        const pg::proof_graph_t *graph, pg::node_idx_t target,
        reachable_map_t *out) const;

    int m_depth_max;
    float m_distance_max, m_redundancy_max;
    bool m_do_disable_reachable_matrix;
};



/* -------- INLINE METHODS -------- */


inline bool a_star_based_enumerator_t::check_permissibility_of(float dist) const
{
    return
        (dist >= 0.0f) and
        (m_max_distance < 0.0f or dist <= m_max_distance);
}


inline bool a_star_based_enumerator_t::check_permissibility_of(const reachability_t &r) const
{
    return check_permissibility_of(r.distance());
}


inline a_star_based_enumerator_t::reachability_t::reachability_t()
: node_from(-1), node_to(-1), dist_from(0.0), dist_to(0.0)
{}


inline a_star_based_enumerator_t::reachability_t::reachability_t(
    const pg::chain_candidate_t &cand,
    pg::node_idx_t i_from, pg::node_idx_t i_to, float d_from, float d_to)
    : pg::chain_candidate_t(cand),
    node_from(i_from), node_to(i_to), dist_from(d_from), dist_to(d_to)
{}


}

}


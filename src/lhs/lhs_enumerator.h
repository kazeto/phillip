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
        virtual lhs_enumerator_t* operator()(const phillip_main_t*) const override;
    };

    a_star_based_enumerator_t(
        const phillip_main_t *ptr, float max_dist, int max_depth = -1);
    virtual lhs_enumerator_t* duplicate(const phillip_main_t *ptr) const;

    virtual pg::proof_graph_t* execute() const;

    virtual bool is_available(std::list<std::string>*) const;
    virtual std::string repr() const;
    virtual bool do_keep_validity_on_timeout() const override { return true; }

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
        virtual lhs_enumerator_t* operator()(const phillip_main_t*) const override;
    };

    depth_based_enumerator_t(const phillip_main_t *ptr, int max_depth);
    virtual lhs_enumerator_t* duplicate(const phillip_main_t *ptr) const;

    virtual pg::proof_graph_t* execute() const;

    virtual bool is_available(std::list<std::string>*) const;
    virtual std::string repr() const;
    virtual bool do_keep_validity_on_timeout() const override { return true; }

private:
    struct reachability_t { float distance, redundancy; };
    typedef hash_map<pg::node_idx_t, reachability_t > reachable_map_t;

    int m_depth_max;
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


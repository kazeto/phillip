#pragma once

#include <set>
#include <tuple>

#include "../phillip.h"


namespace phil
{

/** A namespace about factories of latent-hypotheses-sets. */
namespace lhs
{


/** A class to create latent-hypotheses-set of abduction.
 *  Creation is limited with depth. */
class basic_lhs_enumerator_t : public lhs_enumerator_t
{
public:
    basic_lhs_enumerator_t(
        bool do_deduction, bool do_abduction,
        int max_depth, float max_distance, float max_redundancy);
    virtual pg::proof_graph_t* execute() const;
    virtual bool is_available(std::list<std::string>*) const;
    virtual std::string repr() const;
    
private:
    struct reachability_t { float distance, reduncdancy; };

    /** A struct of reachability to a node.
     *  The key is index of the target node.
     *  The first value of a pair is minimum distance to the target node.
     *  The second value of a pair is redundancy of the target node.*/
    typedef hash_map<pg::node_idx_t, reachability_t > reachable_map_t;

    /** Creates reachability map for observations.
     *  Please call this method after adding observations to graph. */
    hash_map<pg::node_idx_t, reachable_map_t>
        compute_reachability_of_observations(
        const pg::proof_graph_t *graph) const;

    /** Compute reachability of new nodes and
     *  returns possiblity of this chaining. */
    bool compute_reachability_of_chaining(
        const pg::proof_graph_t *graph,
        const hash_map<pg::node_idx_t, reachable_map_t> &reachability,
        const std::vector<pg::node_idx_t> &from,
        const lf::axiom_t &axiom, bool is_forward,
        std::vector<reachable_map_t> *out) const;

    /** Gets candidates of chains from nodes
     *  whose depth is equals to given depth. */
    std::set<pg::chain_candidate_t> enumerate_chain_candidates(
        pg::proof_graph_t *graph, int depth) const;

    /** This is a sub-routine of enumerate_chain_candidate.
     *  In each tuple, the first value is the axiom-id of the chaining,
     *  the second value is whether chaining is forward or not. */
    std::set<std::tuple<axiom_id_t, bool> > enumerate_applicable_axioms(
        pg::proof_graph_t *graph, int depth) const;

    void print_chain_for_debug(
        const pg::proof_graph_t *graph, const lf::axiom_t &axiom,
        const pg::chain_candidate_t &cand, pg::hypernode_idx_t to) const;
    
    bool m_do_deduction, m_do_abduction;
    int m_depth_max;
    float m_distance_max, m_redundancy_max;
};


}

}


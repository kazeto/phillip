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
    basic_lhs_enumerator_t(bool do_deduction = true, bool do_abduction = true);
    virtual pg::proof_graph_t* execute() const;
    virtual bool is_available(std::list<std::string>*) const;
    virtual std::string repr() const;
    
private:
    std::set<pg::chain_candidate_t> enumerate_chain_candidates(
        pg::proof_graph_t *graph, int depth) const;

    /** This is a sub-routine of enumerate_chain_candidate.
     *  In each tuple, the first value is the axiom-id of the chaining,
     *  the second value is whether chaining is forward or not. */
    std::set<std::tuple<axiom_id_t, bool> > enumerate_applicable_axioms(
        pg::proof_graph_t *graph, int depth) const;
    
    bool m_do_deduction, m_do_abduction;
    int m_depth_max;
};


}

}


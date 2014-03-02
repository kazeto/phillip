#pragma once

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
    virtual bool can_execute(std::list<std::string>*) const;
    virtual std::string repr() const;
    
private:
    /** Perform backward-chaining from given node.
     *  This method is sub-routine of execute(). */
    void chain(pg::node_idx_t idx, pg::proof_graph_t *graph) const;
    
    bool m_do_deduction, m_do_abduction;
    int m_depth_max;
};


}

}


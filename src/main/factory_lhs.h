#pragma once

#include "./phillip.h"


namespace phil
{

/** A namespace about factories of latent-hypotheses-sets. */
namespace flhs
{


/** A LHS factory which limitize inference with depth. */
class depth_based_factory_t : public latent_hypotheses_set_factory_interface
{
public:
    depth_based_factory_t();
    virtual pg::proof_graph_t* execute() const;
    virtual bool is_available(std::list<std::string> *error_messages) const;
    virtual std::string repr() const;
    
private:
    int m_depth;
};


}

}


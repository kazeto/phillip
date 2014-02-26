#include "./interface.h"
#include "./phillip.h"


namespace phil
{


void lhs_enumerator_t::add_observations(
    pg::proof_graph_t *target )
{
    std::vector<const literal_t*> literals =
        sys()->get_observation()->get_all_literals();
    
    for( unsigned i=0; i<literals.size(); ++i )
    {
        target->add_observation( *literals[i] );
    }
}


}

#include "./interface.h"
#include "./phillip.h"


namespace phil
{


void lhs_enumerator_t::add_observations(
    pg::proof_graph_t *target )
{
    std::vector<const literal_t*> obs =
        sys()->get_observation()->get_all_literals();

    for (auto it = obs.begin(); it != obs.end(); ++it)
        target->add_observation(**it);

    const lf::logical_function_t *lf_req = sys()->get_requirement();
    if (lf_req != NULL)
    {
        std::vector<const literal_t*> req = lf_req->get_all_literals();

        for (auto it = req.begin(); it != req.end(); ++it)
        if (not (*it)->is_equality())
            target->add_label(**it);
    }
}


bool lhs_enumerator_t::do_include_requirement(
    const pg::proof_graph_t *graph, const std::vector<index_t> &nodes)
{
    for (auto it = nodes.begin(); it != nodes.end(); ++it)
    if (graph->node(*it).type() == pg::NODE_REQUIRED)
        return true;
    return false;
}


}

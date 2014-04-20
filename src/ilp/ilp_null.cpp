/* -*- coding:utf-8 -*- */

#include "./ilp_converter.h"


namespace phil
{

namespace ilp
{


ilp::ilp_problem_t* null_converter_t::execute() const
{
    const pg::proof_graph_t* graph = sys()->get_latent_hypotheses_set();
    ilp::ilp_problem_t *out = new ilp::ilp_problem_t(
        graph, new ilp::basic_solution_interpreter_t(), true);
    return out;
}


bool null_converter_t::is_available(std::list<std::string> *messages) const
{
    return true;
}


std::string null_converter_t::repr() const
{
    return "Plain-LP-Problem-Factory";
}


}

}

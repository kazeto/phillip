#include "./ilp_converter.h"

namespace phil
{

namespace ilp
{


ilp::ilp_problem_t* weighted_converter_t::execute() const
{
    ilp::ilp_problem_t *prob =
        new ilp::ilp_problem_t(sys()->get_latent_hypotheses_set());

    return prob;
}


bool weighted_converter_t::is_available(std::list<std::string> *message) const
{
    return true;
}


std::string weighted_converter_t::repr() const
{
    return "WeightedConverter";
}


}

}
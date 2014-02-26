/* -*- coding:utf-8 -*- */

#include "./ilp_solver.h"

namespace phil
{

namespace sol
{

void null_solver_t::execute(std::vector<ilp::ilp_solution_t> *out) const
{
}


bool null_solver_t::can_execute(
    std::list<std::string> *error_messages ) const
{
    return true;
}


std::string null_solver_t::repr() const
{
    return "Plain-Solution-Factory";
}


}

}

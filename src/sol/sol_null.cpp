/* -*- coding:utf-8 -*- */

#include "./ilp_solver.h"

namespace phil
{

namespace sol
{

void null_solver_t::execute(std::vector<ilp::ilp_solution_t> *out) const
{}


ilp_solver_t* null_solver_t::duplicate(phillip_main_t *ptr) const
{
    return new null_solver_t(ptr);
}


bool null_solver_t::is_available(
    std::list<std::string> *error_messages ) const
{
    return true;
}


std::string null_solver_t::repr() const
{
    return "Null-Solver";
}


}

}

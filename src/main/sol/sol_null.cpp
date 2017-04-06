/* -*- coding:utf-8 -*- */

#include "./ilp_solver.h"

namespace phil
{

namespace sol
{

void null_solver_t::execute(std::vector<ilp::ilp_solution_t> *out) const
{}


bool null_solver_t::is_available(
    std::list<std::string> *error_messages ) const
{
    return true;
}


void null_solver_t::write(std::ostream *os) const
{
    (*os) << "<solver name=\"null\"></solver>" << std::endl;
}


ilp_solver_t* null_solver_t::generator_t::operator()(const phillip_main_t *ph) const
{
    return new sol::null_solver_t(ph);
}


}

}
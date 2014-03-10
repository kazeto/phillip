#ifndef INCLUDE_HENRY_FACTOTY_SOL_H
#define INCLUDE_HENRY_FACTOTY_SOL_H

#include <glpk.h>

#include "./phillip.h"

namespace phil
{

/** A namespace about factories of solution-hypotheses. */
namespace fsol
{


class plain_factory_t : public solver_interface_t
{
public:
    plain_factory_t() {}
    virtual std::vector<lp::solution_t>* execute() const;
    virtual bool is_available( std::list<std::string> *error_messages ) const;
    virtual std::string repr() const;
};


/** A class of solver with GLPK. */
class gnu_linear_programming_kit_t : public solver_interface_t
{
public:
    gnu_linear_programming_kit_t();
    virtual std::vector<lp::solution_t>* execute() const;
    virtual bool is_available( std::list<std::string> *error_messages ) const;
    virtual std::string repr() const;

private:
    void setup( glp_prob *lp ) const;
};

}

}


#endif

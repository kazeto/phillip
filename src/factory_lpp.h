#pragma once

#include "./phillip.h"

namespace phil
{

/** A namespace about factories of linear-programming-problems. */
namespace flpp
{


class plain_factory_t : public linear_programming_problem_factory_interface
{
public:
    plain_factory_t() {}
    virtual lp::linear_programming_problem_t* execute() const;
    virtual bool is_available( std::list<std::string> *error_messages ) const;
    virtual std::string repr() const;
    virtual bool do_repeat() const { return false; }
};

}

}


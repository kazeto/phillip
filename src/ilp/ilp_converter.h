#pragma once

#include "../phillip.h"

namespace phil
{

/** A namespace about factories of linear-programming-problems. */
namespace ilp
{


class null_converter_t : public ilp_converter_t
{
public:
    null_converter_t() {}
    virtual ilp::ilp_problem_t* execute() const;
    virtual bool can_execute( std::list<std::string> *error_messages ) const;
    virtual std::string repr() const;
    virtual bool do_repeat() const { return false; }
};

}

}


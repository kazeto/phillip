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
    virtual bool is_available(std::list<std::string>*) const;
    virtual std::string repr() const;
};


class weighted_converter_t : public ilp_converter_t
{
public:
    weighted_converter_t() {}
    virtual ilp::ilp_problem_t* execute() const;
    virtual bool is_available(std::list<std::string>*) const;
    virtual std::string repr() const;
};

}

}


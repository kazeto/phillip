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


class costed_converter_t : public ilp_converter_t
{
public:
    struct cost_provider_t {
        virtual double operator()(
        const pg::proof_graph_t*, pg::edge_idx_t) const = 0;
    };

    struct basic_cost_provider_t : public cost_provider_t {
        virtual double operator()(
        const pg::proof_graph_t*, pg::edge_idx_t) const;
    };

    costed_converter_t(cost_provider_t *ptr = NULL);
    ~costed_converter_t();

    virtual ilp::ilp_problem_t* execute() const;
    virtual bool is_available(std::list<std::string>*) const;
    virtual std::string repr() const;

private:
    cost_provider_t *m_cost_provider;
};

}

}


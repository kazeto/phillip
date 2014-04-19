/* -*- coding: utf-8 -*- */

#pragma once

#include <vector>
#include "./define.h"


namespace phil
{


namespace pg
{
class proof_graph_t;
}

namespace ilp
{
class ilp_problem_t;
class ilp_solution_t;
}


/** An interface of function class to make latent-hypotheses-set(LHS). */
class lhs_enumerator_t : public henry_component_interface_t
{
public:
    virtual ~lhs_enumerator_t() {}
    virtual pg::proof_graph_t* execute() const = 0;
    
protected:
    /** Add nodes of observations in phillip_main_t to LHS. */
    static void add_observations(pg::proof_graph_t *target);
};


/** An interface of function class to convert LHS into ILP-problem. */
class ilp_converter_t : public henry_component_interface_t
{
public:
    virtual ~ilp_converter_t() {}
    virtual ilp::ilp_problem_t* execute() const = 0;
};


/** An interface of function class to output a solution hypothesis. */
class ilp_solver_t : public henry_component_interface_t
{
public:
    virtual ~ilp_solver_t() {}
    virtual void execute(std::vector<ilp::ilp_solution_t> *out) const = 0;
};


}



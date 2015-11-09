/* -*- coding: utf-8 -*- */

#pragma once

#include <vector>
#include <chrono>

#include "./define.h"
#include "./ilp_problem.h"


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
class lhs_enumerator_t : public phillip_component_interface_t
{
public:
    lhs_enumerator_t(const phillip_main_t *ptr) : phillip_component_interface_t(ptr) {}

    virtual ~lhs_enumerator_t() {}
    virtual lhs_enumerator_t* duplicate(const phillip_main_t *ptr) const = 0;
    virtual pg::proof_graph_t* execute() const = 0;
    
protected:
    static bool do_include_requirement(
        const pg::proof_graph_t *graph, const std::vector<index_t> &nodes);
    static bool do_exceed_max_lhs_size(
        const pg::proof_graph_t *graph, int max_nodes_num);

    /** Add nodes of observations in phillip_main_t to LHS. */
    void add_observations(pg::proof_graph_t *target) const;
    bool do_time_out(const std::chrono::system_clock::time_point &begin) const;

    int get_max_lhs_size() const;
};


/** An interface of function class to convert LHS into ILP-problem. */
class ilp_converter_t : public phillip_component_interface_t
{
public:
    struct enumeration_stopper_t
    {
        virtual bool operator()(const pg::proof_graph_t*) const { return false; }
    };

    ilp_converter_t(const phillip_main_t *ptr) : phillip_component_interface_t(ptr) {}

    virtual ~ilp_converter_t() {}
    virtual ilp_converter_t* duplicate(const phillip_main_t *ptr) const = 0;
    virtual ilp::ilp_problem_t* execute() const = 0;

    /** Tunes its own parameters from a system output and a gold output. */
    virtual void train(
        const ilp::ilp_solution_t &sys, const ilp::ilp_solution_t &gold,
        util::xml_element_t *out) {};

protected:
    /** Converts proof-graph's structure into ILP problem. */
    void convert_proof_graph(ilp::ilp_problem_t *prob) const;
    bool do_time_out(const std::chrono::system_clock::time_point &begin) const;
};


/** An interface of function class to output a solution hypothesis. */
class ilp_solver_t : public phillip_component_interface_t
{
public:
    ilp_solver_t(const phillip_main_t *ptr) : phillip_component_interface_t(ptr) {}

    virtual ~ilp_solver_t() {}
    virtual ilp_solver_t* duplicate(const phillip_main_t *ptr) const = 0;
    virtual void execute(std::vector<ilp::ilp_solution_t> *out) const = 0;

    virtual void solve(
        const ilp::ilp_problem_t *prob,
        std::vector<ilp::ilp_solution_t> *out) const = 0;

protected:
    bool do_time_out(const std::chrono::system_clock::time_point &begin) const;

    /** Infer solution-type of this solution. */
    ilp::solution_type_e infer_solution_type(
        bool has_timed_out_lhs, bool has_timed_out_ilp, bool has_timed_out_sol) const;
};


}



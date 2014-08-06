#ifndef INCLUDE_HENRY_FACTOTY_SOL_H
#define INCLUDE_HENRY_FACTOTY_SOL_H

#include <vector>
#include <string>

#include "../phillip.h"

#ifdef USE_GLPK
#include <glpk.h>
#endif

#ifdef USE_LP_SOLVE
#include <lp_lib.h>
#endif

#ifdef USE_GUROBI
#include <gurobi_c++.h>
#endif


namespace phil
{

/** A namespace about factories of solution-hypotheses. */
namespace sol
{


class null_solver_t : public ilp_solver_t
{
public:
    null_solver_t(phillip_main_t *ptr) : ilp_solver_t(ptr) {}
    virtual void execute(std::vector<ilp::ilp_solution_t> *out) const;
    virtual bool is_available( std::list<std::string> *error_messages ) const;
    virtual std::string repr() const;
};


/** A class of ilp_solver with LP-Solve. */
class lp_solve_t : public ilp_solver_t
{
public:
    lp_solve_t(phillip_main_t *ptr) : ilp_solver_t(ptr) {}
    virtual void execute(std::vector<ilp::ilp_solution_t> *out) const;
    virtual bool is_available(std::list<std::string> *error_messages) const;
    virtual std::string repr() const;

#ifdef USE_LP_SOLVE
private:
    void initialize(
        const ilp::ilp_problem_t *prob, ::lprec **rec, bool do_cpi) const;
    void add_constraint(
        const ilp::ilp_problem_t *prob, ilp::constraint_idx_t idx,
        ::lprec **rec) const;
#endif
};


/** A class of ilp_solver with Gurobi-optimizer. */
class gurobi_t : public ilp_solver_t
{
public:
    gurobi_t(phillip_main_t *ptr, int thread_num, bool do_output_log);

    virtual void execute(std::vector<ilp::ilp_solution_t> *out) const;
    virtual bool is_available(std::list<std::string> *error_messages) const;
    virtual std::string repr() const;

private:
#ifdef USE_GUROBI
    void add_variables(
        GRBModel *model, hash_map<ilp::variable_idx_t, GRBVar> *vars) const;
    void add_constraint(
        GRBModel *model, ilp::constraint_idx_t idx,
        const hash_map<ilp::variable_idx_t, GRBVar> &vars) const;
    ilp::ilp_solution_t convert(
        GRBModel *model, const hash_map<ilp::variable_idx_t, GRBVar> &vars) const;
#endif
    int m_thread_num;
    bool m_do_output_log;
};


}

}


#endif

#ifndef INCLUDE_HENRY_FACTOTY_SOL_H
#define INCLUDE_HENRY_FACTOTY_SOL_H

#include <vector>
#include <string>

#include "../phillip.h"

#ifdef USE_GLPK
#include <glpk.h>
#endif

#ifdef USE_LP_SOLVE
#include "../lib/lpsolve55/lp_lib.h"
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
    null_solver_t() {}
    virtual void execute(std::vector<ilp::ilp_solution_t> *out) const;
    virtual bool can_execute( std::list<std::string> *error_messages ) const;
    virtual std::string repr() const;
};


/** A class of ilp_solver with GLPK. */
class gnu_linear_programming_kit_t : public ilp_solver_t
{
public:
    gnu_linear_programming_kit_t();
    virtual void execute(std::vector<ilp::ilp_solution_t> *out) const;
    virtual bool can_execute( std::list<std::string> *error_messages ) const;
    virtual std::string repr() const;

#ifdef USE_GLPK
private:
    void setup( glp_prob *ilp ) const;
#endif
};


/** A class of ilp_solver with LP-Solve. */
class lp_solve_t : public ilp_solver_t
{
public:
    lp_solve_t() {}
    virtual void execute(std::vector<ilp::ilp_solution_t> *out) const;
    virtual bool can_execute(std::list<std::string> *error_messages) const;
    virtual std::string repr() const;

#ifdef USE_LP_SOLVE
private:
    void initialize(const ilp::ilp_problem_t *prob, ::lprec **rec) const;
    void add_constraint(
        const ilp::ilp_problem_t *prob, ilp::constraint_idx_t idx,
        ::lprec **rec) const;
#endif
};


class gurobi_t : public ilp_solver_t
{
public:
    gurobi_t() {}
    virtual void execute(std::vector<ilp::ilp_solution_t> *out) const;
    virtual bool can_execute(std::list<std::string> *error_messages) const;
    virtual std::string repr() const;

#ifdef USE_GUROBI
private:
    void add_variables(
        GRBModel *model, hash_map<ilp::variable_idx_t, GRBVar> *vars) const;
    void add_constraints(
        GRBModel *model, hash_map<ilp::variable_idx_t, GRBVar> *vars) const;
#endif
};


}

}


#endif

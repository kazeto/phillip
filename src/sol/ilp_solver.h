#ifndef INCLUDE_HENRY_FACTOTY_SOL_H
#define INCLUDE_HENRY_FACTOTY_SOL_H

#include <vector>
#include <string>
#include <memory>

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


/** A class of ilp-solver which does nothing. */
class null_solver_t : public ilp_solver_t
{
public:
    struct generator_t : public component_generator_t<ilp_solver_t>
    {
        virtual ilp_solver_t* operator()(const phillip_main_t*) const override;
    };

    null_solver_t(const phillip_main_t *ptr) : ilp_solver_t(ptr) {}
    virtual ilp_solver_t* duplicate(const phillip_main_t *ptr) const;

    virtual void execute(std::vector<ilp::ilp_solution_t> *out) const;
    virtual void solve(
        const ilp::ilp_problem_t *prob,
        std::vector<ilp::ilp_solution_t> *out) const {};

    virtual bool is_available(std::list<std::string> *error_messages) const;
    virtual std::string repr() const;
    virtual bool do_keep_validity_on_timeout() const override { return false; }
};


/** A class of ilp_solver with LP-Solve. */
class lp_solve_t : public ilp_solver_t
{
public:
    struct generator_t : public component_generator_t<ilp_solver_t>
    {
        virtual ilp_solver_t* operator()(const phillip_main_t*) const override;
    };

    lp_solve_t(const phillip_main_t *ptr) : ilp_solver_t(ptr) {}
    virtual ilp_solver_t* duplicate(const phillip_main_t *ptr) const;

    virtual void execute(std::vector<ilp::ilp_solution_t> *out) const;
    virtual void solve(
        const ilp::ilp_problem_t *prob,
        std::vector<ilp::ilp_solution_t> *out) const;

    virtual bool is_available(std::list<std::string> *error_messages) const;
    virtual std::string repr() const;
    virtual bool do_keep_validity_on_timeout() const override { return false; }

#ifdef USE_LP_SOLVE
private:
    void initialize(
        const ilp::ilp_problem_t *prob, ::lprec **rec) const;
    void add_constraint(
        const ilp::ilp_problem_t *prob, ilp::constraint_idx_t idx,
        ::lprec **rec) const;
#endif
};


/** A class of ilp_solver with Gurobi-optimizer. */
class gurobi_t : public ilp_solver_t
{
public:
    struct generator_t : public component_generator_t<ilp_solver_t>
    {
        virtual ilp_solver_t* operator()(const phillip_main_t*) const override;
    };

    gurobi_t(const phillip_main_t *ptr, int thread_num, bool do_output_log);
    virtual ilp_solver_t* duplicate(const phillip_main_t *ptr) const;

    virtual void execute(std::vector<ilp::ilp_solution_t> *out) const;
    virtual void solve(
        const ilp::ilp_problem_t *prob,
        std::vector<ilp::ilp_solution_t> *out) const;

    virtual bool is_available(std::list<std::string> *error_messages) const;
    virtual std::string repr() const { return "gurobi-optimizer"; }
    virtual bool do_keep_validity_on_timeout() const override { return false; }

protected:
#ifdef USE_GUROBI
    struct model_t
    {
        model_t(const ilp::ilp_problem_t *p) : prob(p) {}

        std::chrono::time_point<std::chrono::system_clock> begin;
        const ilp::ilp_problem_t *prob;
        std::unique_ptr<GRBModel> model;
        std::unique_ptr<GRBEnv> env;
        hash_map<ilp::variable_idx_t, GRBVar> vars;
        hash_set<ilp::constraint_idx_t> lazy_cons;
        bool do_cpi;
    };

    void prepare(model_t&) const;
    ilp::ilp_solution_t optimize(model_t&) const;

    double get_timeout(std::chrono::time_point<std::chrono::system_clock> begin) const;

    void add_variables(
        const ilp::ilp_problem_t *prob,
        GRBModel *model, hash_map<ilp::variable_idx_t, GRBVar> *vars) const;
    void add_constraint(
        GRBModel *model, ilp::constraint_t cons,
        const hash_map<ilp::variable_idx_t, GRBVar> &vars) const;

    ilp::ilp_solution_t convert(
        const ilp::ilp_problem_t *prob,
        GRBModel *model, const hash_map<ilp::variable_idx_t, GRBVar> &vars,
        const std::string &name = "") const;
#endif
    int m_thread_num;
    bool m_do_output_log;
};


/** A class of ilp_solver which outputs k-best solutions with Gurobi-optimizer. */
class gurobi_k_best_t : public gurobi_t
{
public:
    struct generator_t : public component_generator_t<ilp_solver_t>
    {
        virtual ilp_solver_t* operator()(const phillip_main_t*) const override;
    };

    gurobi_k_best_t(
        const phillip_main_t *ptr, int thread_num, bool do_output_log,
        int max_num, float threshold, int margin);

    virtual void execute(std::vector<ilp::ilp_solution_t> *out) const;
    virtual void solve(
        const ilp::ilp_problem_t *prob,
        std::vector<ilp::ilp_solution_t> *out) const;

    virtual bool is_available(std::list<std::string> *error_messages) const;
    virtual std::string repr() const { return "gurobi-k-best"; }
    virtual bool do_keep_validity_on_timeout() const override { return false; }

private:
    int m_max_num;
    float m_threshold;
    int m_margin;
};


}

}


#endif

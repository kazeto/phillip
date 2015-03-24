/* -*- coding:utf-8 -*- */

#include "./ilp_solver.h"


namespace phil
{

namespace sol
{


ilp_solver_t* lp_solve_t::duplicate(phillip_main_t *ptr) const
{
    return new lp_solve_t(ptr);
}


void lp_solve_t::execute(
    std::vector<ilp::ilp_solution_t> *out ) const
{
#ifdef USE_LP_SOLVE
    const ilp::ilp_problem_t *prob = phillip()->get_ilp_problem();
    solve(prob, out);
#endif
}


void lp_solve_t::solve(
    const ilp::ilp_problem_t *prob, std::vector<ilp::ilp_solution_t> *out) const
{
#ifdef USE_LP_SOLVE
    std::vector<double> vars(prob->variables().size(), 0);
    ::lprec *rec(NULL);
    std::time_t begin, now;

    std::time(&begin);
    initialize(prob, &rec);
    
    int ret = ::solve(rec);
    ilp::ilp_solution_t *sol = NULL;
    bool do_break(false), is_timeout(false);

    if (ret == OPTIMAL or ret == SUBOPTIMAL)
    {
        ::get_variables(rec, &vars[0]);
        ilp::solution_type_e type = (ret == OPTIMAL) ?
            ilp::SOLUTION_OPTIMAL : ilp::SOLUTION_SUB_OPTIMAL;
        sol = new ilp::ilp_solution_t(prob, type, vars);
    }

    if (phillip() != NULL)
    {
        std::time(&now);
        int t_sol(now - begin);
        int t_all(
            phillip()->get_time_for_lhs() +
            phillip()->get_time_for_ilp() + t_sol);

        if (phillip()->is_timeout_sol(t_sol) or phillip()->is_timeout_all(t_all))
            is_timeout = true;
    }

    ::delete_lp(rec);

    if (sol == NULL)
        sol = new ilp::ilp_solution_t(
        prob, ilp::SOLUTION_NOT_AVAILABLE,
        std::vector<double>(0.0, prob->variables().size()));

    sol->timeout(is_timeout);
    out->push_back(*sol);

    delete sol;
#endif
}



bool lp_solve_t::is_available(std::list<std::string> *messages) const
{
#ifdef USE_LP_SOLVE
    return true;
#else
    return false;
#endif
}


std::string lp_solve_t::repr() const
{
    return "LP-Solve";
}


#ifdef USE_LP_SOLVE

#ifdef _WIN32
typedef void(__WINAPI lphandlestr_func)(::lprec *lp, void *userhandle, char *buf);


void __WINAPI lp_handler(::lprec *lp, void *userhandle, char *buf)
#else
void lp_handler(::lprec *lp, void *userhandle, char *buf)
#endif
{
    std::string line(buf);
    int i(0);
    for (int j = 0; j < line.length(); ++j)
    {
        if (line.at(j) == '\n')
        {
            if (j - i > 0 and not (j - i == 1 and line.at(i) == ' '))
                print_console(line.substr(i, j - i) + "$");
            i = j + 1;
        }
    }
}


void lp_solve_t::initialize(const ilp::ilp_problem_t *prob, ::lprec **rec) const
{
    const std::vector<ilp::variable_t> &variables = prob->variables();
    const std::vector<ilp::constraint_t> &constraints = prob->constraints();
    const hash_set<ilp::constraint_idx_t>
        &lazy_cons = prob->get_lazy_constraints();

    // SETS OBJECTIVE FUNCTIONS.
    std::vector<double> vars(variables.size() + 1, 0);
    for (size_t i = 0; i < variables.size(); ++i)
        vars[i+1] = variables.at(i).objective_coefficient();

    *rec = ::make_lp(0, variables.size());
    ::set_obj_fn(*rec, &vars[0]);
    prob->do_maximize() ?
        ::set_maxim(*rec) : ::set_minim(*rec);

    if (phillip() != NULL)
    if (phillip()->timeout_sol() > 0)
        ::set_timeout(*rec, phillip()->timeout_sol());
    
    ::set_outputfile(*rec, "");
    ::put_logfunc(*rec, lp_handler, NULL);

    // SETS ALL VARIABLES TO INTEGER.
    for (size_t i = 0; i < variables.size(); ++i)
    {
        ::set_int(*rec, i + 1, true);
        ::set_upbo(*rec, i + 1, 1.0);
    }

    // ADDS CONSTRAINTS.
    for (size_t i = 0; i < constraints.size(); ++i)
        add_constraint(prob, i, rec);

    // ADDS CONSTRAINTS FOR CONSTANTS.
    const hash_map<ilp::variable_idx_t, double>
        &consts = prob->const_variable_values();
    for (auto it = consts.begin(); it != consts.end(); ++it)
    {
        std::vector<double> vec(variables.size() + 1, 0.0);
        vec[it->first + 1] = it->second;
        ::add_constraint(*rec, &vec[0], EQ, it->second);
    }
}


void lp_solve_t::add_constraint(
    const ilp::ilp_problem_t *prob, ilp::constraint_idx_t idx,
    ::lprec **rec) const
{
    const std::vector<ilp::variable_t> &variables = prob->variables();
    const std::vector<ilp::constraint_t> &constraints = prob->constraints();
    const ilp::constraint_t &con = constraints.at(idx);
    std::vector<double> vec(variables.size() + 1, 0.0);

    for( auto it = con.terms().begin(); it != con.terms().end(); ++it )
        vec[it->var_idx + 1] = it->coefficient;

    switch (con.operator_type())
    {
    case ilp::OPR_EQUAL:
        ::add_constraint(*rec, &vec[0], EQ, con.bound()); break;
    case ilp::OPR_LESS_EQ:
        ::add_constraint(*rec, &vec[0], LE, con.upper_bound()); break;
    case ilp::OPR_GREATER_EQ:
        ::add_constraint(*rec, &vec[0], GE, con.lower_bound()); break;
    case ilp::OPR_RANGE:
        ::add_constraint(*rec, &vec[0], LE, con.upper_bound());
        ::add_constraint(*rec, &vec[0], GE, con.lower_bound());
        break;
    }
}


#endif


ilp_solver_t* lp_solve_t::generator_t::operator()(phillip_main_t *ph) const
{
    return new sol::lp_solve_t(ph);
}


}

}

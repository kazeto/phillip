/* -*- coding:utf-8 -*- */

#include "./ilp_solver.h"


namespace phil
{

namespace sol
{


void lp_solve_t::execute(
    std::vector<ilp::ilp_solution_t> *out ) const
{
#ifdef USE_LP_SOLVE
    const ilp::ilp_problem_t *prob = sys()->get_ilp_problem();
    std::vector<double> vars(prob->variables().size(), 0);
    ::lprec *rec(NULL);
    hash_set<ilp::constraint_idx_t>
        lazy_cons(prob->get_lazy_constraints());
    bool do_break(false);
    bool disable_cutting_plane(sys()->flag("disable_cutting_plane"));
    bool do_cutting_plane(not lazy_cons.empty() and not disable_cutting_plane);
    
    initialize(prob, &rec);
    
    size_t num_loop(0);
    while (not do_break)
    {
        if (do_cutting_plane)
        {
            std::cerr
                << time_stamp() << "begin: cutting-plane loop #"
                << (num_loop++) << std::endl;
        }

        ::solve(rec);
        ::get_variables(rec, &vars[0]);
        ilp::ilp_solution_t sol(prob, ilp::SOLUTION_OPTIMAL, vars);

        if (not lazy_cons.empty() and not disable_cutting_plane)
        {
            hash_set<ilp::constraint_idx_t> filtered;
            sol.filter_unsatisfied_constraints(&lazy_cons, &filtered);
            
            if (not filtered.empty())
            {
                // ADD VIOLATED CONSTRAINTS
                for (auto it = filtered.begin(); it != filtered.end(); ++it)
                    add_constraint(prob, *it, &rec);
            }
            else do_break = true;
        }
        else do_break = true;

        if (do_break)
        {
            ::delete_lp(rec);
            out->push_back(sol);
        }
    }
#endif
}


bool lp_solve_t::can_execute(std::list<std::string> *messages) const
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

void lp_solve_t::initialize(const ilp::ilp_problem_t *prob, ::lprec **rec) const
{
    const std::vector<ilp::variable_t> &variables = prob->variables();
    const std::vector<ilp::constraint_t> &constraints = prob->constraints();
    const hash_set<ilp::constraint_idx_t>
        &lazy_cons = prob->get_lazy_constraints();
    bool disable_cutting_plane(sys()->flag("disable_cutting_plane"));

    // SET OBJECTIVE FUNCTIONS
    std::vector<double> vars(variables.size() + 1, 0);
    for (size_t i = 0; i < variables.size(); ++i)
        vars[i+1] = variables.at(i).objective_coefficient();

    *rec = ::make_lp(0, variables.size());
    ::set_obj_fn(*rec, &vars[0]);
    ::set_maxim(*rec);

    // SET ALL VARIABLES TO INTEGER
    for (size_t i = 0; i < variables.size(); ++i)
    {
        ::set_int(*rec, i + 1, true);
        ::set_upbo(*rec, i + 1, 1.0);
    }

    // ADD NOT LAZY CONSTRAINTS
    for (size_t i = 0; i < constraints.size(); ++i)
    {
        if (lazy_cons.count(i) == 0 or disable_cutting_plane)
            add_constraint(prob, i, rec);
    }

    // ADD CONSTRAINTS FOR CONSTS
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


}

}

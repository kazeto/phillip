#include <mutex>
#include <algorithm>
#include "./ilp_solver.h"


namespace phil
{

namespace sol
{


#define GRBEXECUTE(x) \
    try { x; } \
    catch (GRBException e) { \
        util::print_error_fmt("Gurobi: code(%d): %s", \
            e.getErrorCode(), e.getMessage().c_str()); }


std::mutex g_mutex_gurobi;


gurobi_t::gurobi_t(const phillip_main_t *ptr, int thread_num, bool do_output_log)
    : ilp_solver_t(ptr), m_thread_num(thread_num), m_do_output_log(do_output_log)
{
    if (m_thread_num <= 0)
        m_thread_num = 1;
}


ilp_solver_t* gurobi_t::duplicate(const phillip_main_t *ptr) const
{
    return new gurobi_t(ptr, m_thread_num, m_do_output_log);
}


void gurobi_t::execute(std::vector<ilp::ilp_solution_t> *out) const
{
#ifdef USE_GUROBI
    const ilp::ilp_problem_t *prob = phillip()->get_ilp_problem();
    solve(prob, out);
#else
    out->push_back(ilp::ilp_solution_t(
        prob, ilp::SOLUTION_NOT_AVAILABLE,
        std::vector<double>(prob->variables().size(), 0.0)));
#endif
}


void gurobi_t::solve(
    const ilp::ilp_problem_t *prob,
    std::vector<ilp::ilp_solution_t> *out) const
{
#ifdef USE_GUROBI
    model_t m(prob);

    prepare(m);
    out->push_back(optimize(m));
#endif
}


bool gurobi_t::is_available(std::list<std::string> *err) const
{
#ifdef USE_GUROBI
    return true;
#else
    err->push_back("This binary cannot use gurobi-optimizer.");
    return false;
#endif
}


#ifdef USE_GUROBI

void gurobi_t::prepare(model_t &m) const
{
    m.begin = std::chrono::system_clock::now();

    g_mutex_gurobi.lock();
    {
        m.env.reset(new GRBEnv());
        m.model.reset(new GRBModel(*m.env));
        m.lazy_cons = m.prob->get_lazy_constraints();
        m.do_cpi = (not m.lazy_cons.empty());
    }
    g_mutex_gurobi.unlock();

    if (phillip() != NULL)
    if (phillip()->flag("disable_cpi"))
        m.do_cpi = false;

    add_variables(m.prob, m.model.get(), &m.vars);

    for (int i = 0; i < m.prob->constraints().size(); ++i)
    if (m.lazy_cons.count(i) == 0 or not m.do_cpi)
        add_constraint(m.model.get(), m.prob->constraint(i), m.vars);

    double timeout = get_timeout(m.begin);

    GRBEXECUTE(m.model->update());
    GRBEXECUTE(m.model->set(
        GRB_IntAttr_ModelSense,
        (m.prob->do_maximize() ? GRB_MAXIMIZE : GRB_MINIMIZE)));
    GRBEXECUTE(
        m.model->getEnv().set(GRB_IntParam_OutputFlag, (m_do_output_log ? 1 : 0)));

    GRBEXECUTE(
    if (m_thread_num > 1)
        (m.model->getEnv().set(GRB_IntParam_Threads, m_thread_num)));

    GRBEXECUTE(
    if (timeout > 0)
        m.model->getEnv().set(GRB_DoubleParam_TimeLimit, timeout));
}


ilp::ilp_solution_t gurobi_t::optimize(model_t &m) const
{
    size_t num_loop(0);
    while (true)
    {
        if (m.do_cpi and phillip_main_t::verbose() >= VERBOSE_1)
            util::print_console_fmt("begin: Cutting-Plane-Inference #%d", (num_loop++));

        GRBEXECUTE(m.model->optimize());

        if (m.model->get(GRB_IntAttr_SolCount) == 0)
        {
            if (m.model->get(GRB_IntAttr_Status) == GRB_INFEASIBLE)
            {
                m.model->computeIIS();
                GRBConstr *cons = m.model->getConstrs();

                for (int i = 0; i < m.model->get(GRB_IntAttr_NumConstrs); ++i)
                if (cons[i].get(GRB_IntAttr_IISConstr) == 1)
                {
                    std::string name(cons[i].get(GRB_StringAttr_ConstrName));
                    util::print_warning("Infeasible: " + name);
                }

                delete[] cons;
            }

            return ilp::ilp_solution_t(
                m.prob, ilp::SOLUTION_NOT_AVAILABLE,
                std::vector<double>(m.prob->variables().size(), 0.0));
        }
        else
        {
            ilp::ilp_solution_t sol =
                convert(m.prob, m.model.get(), m.vars, m.prob->name());
            bool do_break(false);
            bool do_violate_lazy_constraint(false);

            if (not m.lazy_cons.empty() and m.do_cpi)
            {
                hash_set<ilp::constraint_idx_t> filtered;
                sol.filter_unsatisfied_constraints(&m.lazy_cons, &filtered);

                if (not filtered.empty())
                {
                    // ADD VIOLATED CONSTRAINTS
                    for (auto it = filtered.begin(); it != filtered.end(); ++it)
                        add_constraint(m.model.get(), m.prob->constraint(*it), m.vars);
                    GRBEXECUTE(m.model->update());
                    do_violate_lazy_constraint = true;
                }
                else do_break = true;
            }
            else do_break = true;

            if (not do_break and phillip() != NULL)
            {
                if (do_time_out(m.begin))
                {
                    sol.timeout(true);
                    do_break = true;
                }
                else
                {
                    double t_o = get_timeout(m.begin);
                    if (t_o > 0.0)
                        GRBEXECUTE(m.model->getEnv().set(GRB_DoubleParam_TimeLimit, t_o));
                }
            }

            if (do_break)
            {
                bool timeout_lhs =
                    (m.prob->proof_graph() != NULL) ?
                    m.prob->proof_graph()->has_timed_out() : false;
                ilp::solution_type_e sol_type =
                    infer_solution_type(timeout_lhs, m.prob->has_timed_out(), false);
                if (do_violate_lazy_constraint)
                    sol_type = ilp::SOLUTION_NOT_AVAILABLE;

                sol.set_solution_type(sol_type);
                return sol;
            }
        }
    }
}


double gurobi_t::get_timeout(
    std::chrono::time_point<std::chrono::system_clock> begin) const
{
    duration_time_t passed = util::duration_time(begin);
    double t_o_sol(-1), t_o_all(-1);

    if (phillip() != NULL)
    {
        if (not phillip()->timeout_sol().empty())
            t_o_sol = std::max<double>(
            0.01,
            phillip()->timeout_sol().get() - passed);
        if (not phillip()->timeout_all().empty())
            t_o_all = std::max<double>(
            0.01,
            phillip()->timeout_all().get()
            - phillip()->get_time_for_lhs()
            - phillip()->get_time_for_ilp()
            - passed);
    }

    double timeout(-1);

    if (t_o_sol > t_o_all)
        timeout = (t_o_all > 0.0) ? t_o_all : t_o_sol;
    else
        timeout = (t_o_sol > 0.0) ? t_o_sol : t_o_all;

    return (timeout > 0.0) ? timeout : -1.0;
}


void gurobi_t::add_variables(
    const ilp::ilp_problem_t *prob,
    GRBModel *model, hash_map<ilp::variable_idx_t, GRBVar> *vars) const
{
    for (int i = 0; i < prob->variables().size(); ++i)
    {
        const ilp::variable_t &v = prob->variable(i);
        double lb(0.0), ub(1.0);
        
        if (prob->is_constant_variable(i))
            lb = ub = prob->const_variable_value(i);

        GRBEXECUTE(
            (*vars)[i] = model->addVar(
                lb, ub, v.objective_coefficient(),
                (ub - lb == 1.0) ? GRB_BINARY : GRB_INTEGER))
    }

    GRBEXECUTE(model->update())
}


void gurobi_t::add_constraint(
    GRBModel *model, ilp::constraint_t cons,
    const hash_map<ilp::variable_idx_t, GRBVar> &vars) const
{
    std::string name = cons.name().substr(0, 32);
    GRBLinExpr expr;

    for (auto t = cons.terms().begin(); t != cons.terms().end(); ++t)
        expr += t->coefficient * vars.at(t->var_idx);

    GRBEXECUTE(
        switch (cons.operator_type())
    {
        case ilp::OPR_EQUAL:
            model->addConstr(expr, GRB_EQUAL, cons.bound(), name);
            break;
        case ilp::OPR_LESS_EQ:
            model->addConstr(expr, GRB_LESS_EQUAL, cons.upper_bound(), name);
            break;
        case ilp::OPR_GREATER_EQ:
            model->addConstr(expr, GRB_GREATER_EQUAL, cons.lower_bound(), name);
            break;
        case ilp::OPR_RANGE:
            model->addRange(expr, cons.lower_bound(), cons.upper_bound(), name);
            break;
    });
}


ilp::ilp_solution_t gurobi_t::convert(
    const ilp::ilp_problem_t *prob,
    GRBModel *model, const hash_map<ilp::variable_idx_t, GRBVar> &vars,
    const std::string &name) const
{
    std::vector<double> values(prob->variables().size(), 0);
    GRBVar *p_vars = model->getVars();
    double *p_values = model->get(GRB_DoubleAttr_X, p_vars, values.size());

    for (int i = 0; i < prob->variables().size(); ++i)
        values[i] = p_values[i];

    delete p_vars;
    delete p_values;

    return ilp::ilp_solution_t(prob, ilp::SOLUTION_OPTIMAL, values);
}

#endif


ilp_solver_t* gurobi_t::generator_t::operator()(const phillip_main_t *ph) const
{
    return new sol::gurobi_t(
        ph,
        ph->param_int("gurobi_thread_num"),
        ph->flag("activate_gurobi_log"));
}


}

}

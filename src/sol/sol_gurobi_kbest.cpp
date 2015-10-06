#include <mutex>
#include <algorithm>
#include "./ilp_solver.h"


namespace phil
{

namespace sol
{


gurobi_k_best_t::gurobi_k_best_t(
    const phillip_main_t *ptr, int thread_num, bool do_output_log,
    int max_num, float threshold, int margin)
    : gurobi_t(ptr, thread_num, do_output_log),
    m_max_num(max_num), m_threshold(threshold), m_margin(margin)
{}


void gurobi_k_best_t::execute(std::vector<ilp::ilp_solution_t> *out) const
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


void gurobi_k_best_t::solve(
    const ilp::ilp_problem_t *prob,
    std::vector<ilp::ilp_solution_t> *out) const
{
    const pg::proof_graph_t *graph = prob->proof_graph();

#ifdef USE_GUROBI
    model_t m(prob);
    prepare(m);

    while (out->size() < m_max_num)
    {
        if (not out->empty())
        {
            ilp::constraint_t con(
                util::format("margin:sol(%d)", out->size()), ilp::OPR_GREATER_EQ);
            const ilp::ilp_solution_t &sol = out->back();
            int count(0);

            for (pg::node_idx_t i = 0; i < graph->nodes().size(); ++i)
            {
                ilp::variable_idx_t v = prob->find_variable_with_node(i);
                if (v >= 0)
                {
                    if (prob->node_is_active(sol, i))
                    {
                        con.add_term(v, -1.0);
                        ++count;
                    }
                    else
                        con.add_term(v, 1.0);
                }
            }

            con.set_bound((double)(m_margin - count));
            add_constraint(m.model.get(), con, m.vars);
        }

        ilp::ilp_solution_t sol = optimize(m);

        if (out->empty())
        {
            if (sol.type() == ilp::SOLUTION_NOT_AVAILABLE)
                break;

            // IF DELTA IS BIGGER THAN THRESHOLD, THIS SOLUTION IS NOT ACCEPTABLE.
            if (m_threshold >= 0.0)
            {
                double delta =
                    sol.value_of_objective_function() -
                    out->front().value_of_objective_function();
                if (std::abs(delta) > m_threshold)
                    break;
            }
        }

        out->push_back(sol);

        if (sol.type() == ilp::SOLUTION_NOT_AVAILABLE) break;

        // TODO: ‰½‚à„˜_‚µ‚È‚¢ó‘Ô‚æ‚è‚à•]‰¿’l‚ª¬‚³‚¢ê‡‚ÍŠü‹p
    }

#endif
}


bool gurobi_k_best_t::is_available(std::list<std::string> *error_messages) const
{
    if (gurobi_t::is_available(error_messages))
    {
        if (m_max_num < 1)
        {
            error_messages->push_back(
                "gurobi_k_best_t::m_max_num must be bigger than 0.");
            return false;
        }

        if (m_margin < 1)
        {
            error_messages->push_back(
                "gurobi_k_best_t::m_margin must be bigger than 0.");
            return false;
        }
    }

    return true;
}


ilp_solver_t* gurobi_k_best_t::generator_t::operator()(const phillip_main_t *ph) const
{
    return new sol::gurobi_k_best_t(
        ph,
        ph->param_int("gurobi_thread_num"),
        ph->flag("activate_gurobi_log"),
        ph->param_int("max_sols_num", 1),
        ph->param_float("sols_threshold"),
        ph->param_int("sols_margin", 1));
}


}

}

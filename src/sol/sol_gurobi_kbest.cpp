#include <mutex>
#include <algorithm>
#include "./ilp_solver.h"


namespace phil
{

namespace sol
{


gurobi_k_best_t::gurobi_k_best_t(
    phillip_main_t *ptr, int thread_num, bool do_output_log,
    int max_num, float threshold, int margin)
    : gurobi_t(ptr, thread_num, do_output_log),
    m_max_num(max_num), m_threshold(threshold), m_margin(margin)
{}


void gurobi_k_best_t::execute(std::vector<ilp::ilp_solution_t> *out) const
{
	const ilp::ilp_problem_t *prob = phillip()->get_ilp_problem();
#ifdef USE_GUROBI
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
#ifdef USE_GUROBI
    if (phillip_main_t::verbose() >= VERBOSE_3)
    {
        util::print_console("K-best optimization mode:");
        util::print_console_fmt("    max solutions num = %d", m_max_num);
        util::print_console_fmt("    threshold = %02f", m_threshold);
        util::print_console_fmt("    margin = %d", m_margin);
    }

    const pg::proof_graph_t *graph = prob->proof_graph();
    model_t m(prob);
    prepare(m);

    while (out->size() < m_max_num)
    {
        IF_VERBOSE_1(util::format("Optimization #%d", out->size() + 1));

        if (not out->empty())
        {
            ilp::constraint_t con(
                util::format("margin:sol(%d)", out->size()), ilp::OPR_GREATER_EQ);
            const ilp::ilp_solution_t &sol = out->back();
            int count(0);

            for (auto n : graph->nodes())
            if (n.type() == pg::NODE_HYPOTHESIS
                and not n.is_equality_node()
                and not n.is_non_equality_node())
            {
                ilp::variable_idx_t v = prob->find_variable_with_node(n.index());
                if (v >= 0)
                {
                    if (prob->node_is_active(sol, n.index()))
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

        if (not out->empty())
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
        if (sol.has_timed_out()) break;
    }
    IF_VERBOSE_1(util::format("Finish solving: # of solutions = %d", out->size()));

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


ilp_solver_t* gurobi_k_best_t::generator_t::operator()(phillip_main_t *ph) const
{
    return new sol::gurobi_k_best_t(
        ph,
        ph->param_int("gurobi-thread-num"),
        ph->flag("activate-gurobi-log"),
        ph->param_int("max-sols-num", 5),
        ph->param_float("sols-threshold", 10.0),
        ph->param_int("sols-margin", 1));
}


}

}

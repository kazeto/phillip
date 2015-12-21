#include "./interface.h"
#include "./phillip.h"


namespace phil
{


bool lhs_enumerator_t::do_include_requirement(
    const pg::proof_graph_t *graph, const std::vector<index_t> &nodes)
{
    for (auto it = nodes.begin(); it != nodes.end(); ++it)
    if (graph->node(*it).type() == pg::NODE_REQUIRED)
        return true;
    return false;
}


bool lhs_enumerator_t::do_exceed_max_lhs_size(
    const pg::proof_graph_t *graph, int max_nodes_num)
{
    // COUNTS THE NUMBER OF HYPOTHESIS NODES
    int num = 0;
    for (auto n : graph->nodes())
    if (n.depth() > 0) ++num;

    if (max_nodes_num > 0 and num >= max_nodes_num)
    {
        IF_VERBOSE_3("The number of literals exceeds the limitation!");
        IF_VERBOSE_4(util::format("    now: %d", num));
        IF_VERBOSE_4(util::format("    max: %d", max_nodes_num));
        return true;
    }
    else
        return false;
}


void lhs_enumerator_t::add_observations(pg::proof_graph_t *target) const
{
    std::vector<const literal_t*> obs =
        phillip()->get_observation()->get_all_literals();

    for (auto it = obs.begin(); it != obs.end(); ++it)
        target->add_observation(**it);

    const lf::logical_function_t *lf_req = phillip()->get_requirement();
    if (lf_req != NULL)
    {
        for (auto br : lf_req->branches())
            target->add_requirement(br);
    }
}


bool lhs_enumerator_t::do_time_out(const std::chrono::system_clock::time_point &begin) const
{
    return
        phillip()->timeout_lhs().do_time_out(begin) or
        phillip()->timeout_all().do_time_out(begin);
}


int lhs_enumerator_t::get_max_lhs_size() const
{
    return phillip()->param_int("max_lhs_size");
}


void ilp_converter_t::convert_proof_graph(ilp::ilp_problem_t *prob) const
{
    const pg::proof_graph_t *graph = prob->proof_graph();
    auto begin = std::chrono::system_clock::now();

#define _check_timeout if(do_time_out(begin)) { prob->timeout(true); return; }

    // ADD VARIABLES FOR NODES
    for (pg::node_idx_t i = 0; i < graph->nodes().size(); ++i)
    {
        ilp::variable_idx_t var = prob->add_variable_of_node(i);
        if (graph->node(i).type() == pg::NODE_OBSERVABLE or
            graph->node(i).type() == pg::NODE_REQUIRED)
            prob->add_constancy_of_variable(var, 1.0);
        if (i % 100 == 0)
            _check_timeout;
    }

    // ADD VARIABLES FOR HYPERNODES
    for (pg::hypernode_idx_t i = 0; i < graph->hypernodes().size(); ++i)
    {
        prob->add_variable_of_hypernode(i);
        if (i % 100 == 0)
            _check_timeout;
    }

    for (pg::edge_idx_t i = 0; i < graph->edges().size(); ++i)
    {
        prob->add_variable_of_edge(i);
        if (i % 100 == 0)
            _check_timeout;
    }

    // ADD CONSTRAINTS FOR NODES
    for (pg::node_idx_t i = 0; i < graph->nodes().size(); ++i)
    {
        prob->add_constraint_of_dependence_of_node_on_hypernode(i);
        if (i % 100 == 0)
            _check_timeout;
    }

    // ADD CONSTRAINTS FOR HYPERNODES
    for (pg::hypernode_idx_t i = 0; i < graph->hypernodes().size(); ++i)
    {
        prob->add_constraint_of_dependence_of_hypernode_on_parents(i);
        if (i % 100 == 0)
            _check_timeout;
    }

    // ADD CONSTRAINTS FOR CHAINING EDGES
    for (pg::edge_idx_t i = 0; i < graph->edges().size(); ++i)
    {
        prob->add_constrains_of_conditions_for_chain(i);
        if (i % 100 == 0)
            _check_timeout;
    }

    prob->add_variables_for_requirement(false);
    _check_timeout;

    prob->add_constraints_of_mutual_exclusions();
    _check_timeout;

    prob->add_constrains_of_exclusive_chains();
    _check_timeout;

    prob->add_constraints_of_transitive_unifications();
    _check_timeout;
}


opt::training_result_t* ilp_converter_t::train(
    opt::epoch_t epoch, const ilp::ilp_solution_t &sys, const ilp::ilp_solution_t &gold)
{
    return NULL;
}


bool ilp_converter_t::do_time_out(const std::chrono::system_clock::time_point &begin) const
{
    duration_time_t t_ilp = util::duration_time(begin);
    duration_time_t t_all(phillip()->get_time_for_lhs() + t_ilp);

    return
        phillip()->timeout_ilp().do_time_out(t_ilp) or
        phillip()->timeout_all().do_time_out(t_all);
}


bool ilp_solver_t::do_time_out(const std::chrono::system_clock::time_point &begin) const
{
    duration_time_t t_sol = util::duration_time(begin);

    if (phillip() != NULL)
    {
        duration_time_t t_all =
            phillip()->get_time_for_lhs() + phillip()->get_time_for_ilp() + t_sol;

        return
            phillip()->timeout_sol().do_time_out(t_sol) or
            phillip()->timeout_all().do_time_out(t_all);
    }
    else
        return t_sol > 60.0f; // 1 MIN.
}


ilp::solution_type_e ilp_solver_t::infer_solution_type(
    bool has_timed_out_lhs, bool has_timed_out_ilp, bool has_timed_out_sol) const
{
    ilp::solution_type_e out(ilp::SOLUTION_OPTIMAL);

    if (phillip())
    {
        if (has_timed_out_lhs)
        {
            ilp::solution_type_e t =
                phillip()->generator()->do_keep_validity_on_timeout() ?
                ilp::SOLUTION_SUB_OPTIMAL : ilp::SOLUTION_NOT_AVAILABLE;
            if (out < t) out = t;
        }

        if (has_timed_out_ilp)
        {
            ilp::solution_type_e t =
                phillip()->converter()->do_keep_validity_on_timeout() ?
                ilp::SOLUTION_SUB_OPTIMAL : ilp::SOLUTION_NOT_AVAILABLE;
            if (out < t) out = t;
        }

        if (has_timed_out_sol)
        {
            ilp::solution_type_e t =
                phillip()->solver()->do_keep_validity_on_timeout() ?
                ilp::SOLUTION_SUB_OPTIMAL : ilp::SOLUTION_NOT_AVAILABLE;
            if (out < t) out = t;
        }
    }

    return out;
}


}

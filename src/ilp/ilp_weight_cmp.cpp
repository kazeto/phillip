#include <algorithm>
#include "./ilp_converter.h"

namespace phil
{

namespace ilp
{


compressed_weighted_converter_t::compressed_weighted_converter_t(
    double default_obs_cost, weighted_converter_t::weight_provider_t *ptr)
: weighted_converter_t(default_obs_cost, ptr)
{}


ilp::ilp_problem_t* compressed_weighted_converter_t::execute() const
{
    const pg::proof_graph_t *graph = sys()->get_latent_hypotheses_set();
    ilp::ilp_problem_t *prob = new ilp::ilp_problem_t(
        graph, new my_solution_interpreter_t(), false);

    // ADD VARIABLES FOR EQUALITY-NODES & NON-EQUALITY-NODES
    for (pg::node_idx_t i = 0; i < graph->nodes().size(); ++i)
    {
        const pg::node_t &n = graph->node(i);
        pg::hypernode_idx_t mhn = n.master_hypernode();
        if (n.is_equality_node() or n.is_non_equality_node())
            prob->add_variable_of_node(i);
    }

    // ADD VARIABLES FOR HEAD-HYPERNODE OF EACH EDGE
    for (pg::edge_idx_t i = 0; i < graph->edges().size(); ++i)
    {
        const pg::edge_t &e = graph->edge(i);
        if (e.head() >= 0)
            prob->add_variable_of_hypernode(e.head(), 0.0, e.is_unify_edge());
    }

    // ADD CONSTRAINTS FOR EDGES
    for (pg::edge_idx_t i = 0; i < graph->edges().size(); ++i)
        add_constraints_for_edge(graph, prob, i);

    // ADD CONSTRAINTS FOR EXCLUSIVE CHAINS
    prob->add_constrains_of_exclusive_chains();
    prob->add_constraints_of_transitive_unifications();

    // ASSIGN COSTS OF EDGES TO HYPERNODES
    hash_map<pg::node_idx_t, double> node2cost;
    compute_observation_cost(graph, &node2cost);
    compute_hypothesis_cost(graph, &node2cost);
    assign_costs(graph, prob, node2cost);

    prob->add_xml_decorator(new my_xml_decorator_t(node2cost));
    prob->add_attributes("converter", "weighted");

    return prob;
}


void compressed_weighted_converter_t::add_constraints_for_edge(
    const pg::proof_graph_t *graph, ilp::ilp_problem_t *prob, pg::edge_idx_t idx) const
{
    const pg::edge_t &edge = graph->edge(idx);
    auto hn2var = prob->hypernode_to_variable();

    if (edge.head() >= 0)
    {
        const std::vector<pg::node_idx_t> &hn = graph->hypernode(edge.tail());
        hash_set<pg::hypernode_idx_t> masters;
        for (auto n = hn.begin(); n != hn.end(); ++n)
        {
            pg::hypernode_idx_t hn = graph->node(*n).master_hypernode();
            if (hn >= 0) masters.insert(hn);
        }

        if (not masters.empty())
        {
            // IF THE HEAD IS TRUE, NODES IN TAIL MUST BE TRUE.
            constraint_t con(
                format("condition_for_edge:hn(%d)", idx), OPR_GREATER_EQ, 0.0);
            for (auto hn = masters.begin(); hn != masters.end(); ++hn)
            {
                variable_idx_t v = hn2var.at(*hn);
                con.add_term(v, 1.0);
            }
            con.add_term(hn2var.at(edge.head()), -1.0 * masters.size());
            prob->add_constraint(con);
        }
    }

    if (edge.is_chain_edge())
    {
        auto found = hn2var.find(edge.head());
        if (found == hn2var.end()) return;

        hash_set<pg::node_idx_t> conds;
        bool is_available = graph->check_availability_of_chain(idx, &conds);

        // IF THE CHAIN IS NOT AVAILABLE, HEAD-HYPERNODE MUST BE FALSE.
        if (not is_available)
            prob->add_constancy_of_variable(found->second, 0.0);
        else if (not conds.empty())
        {
            // TO PERFORM THE CHAINING, NODES IN conds MUST BE TRUE.
            constraint_t con(
                format("condition_for_chain:e(%d)", idx),
                OPR_GREATER_EQ, 0.0);

            for (auto n = conds.begin(); n != conds.end(); ++n)
            {
                variable_idx_t _v = prob->find_variable_with_node(*n);
                assert(_v >= 0);
                con.add_term(_v, 1.0);
            }

            con.add_term(found->second, -1.0 * con.terms().size());
            prob->add_constraint(con);
        }
    }
}


void compressed_weighted_converter_t::compute_observation_cost(
    const pg::proof_graph_t *graph,
    hash_map<pg::node_idx_t, double> *node2cost) const
{
    const lf::input_t &input = *sys()->get_input();
    assert(input.obs.is_operator(lf::OPR_AND));

    const std::vector<lf::logical_function_t> &obs = input.obs.branches();
    std::vector<double> costs;
    std::vector<pg::node_idx_t> indices;

    for (auto it = obs.begin(); it != obs.end(); ++it)
    {
        double cost(m_default_observation_cost);
        it->param2double(&cost);
        costs.push_back(cost);
    }

    for (int i = 0; i < graph->nodes().size() and not costs.empty(); ++i)
    if (graph->node(i).type() == pg::NODE_OBSERVABLE)
        indices.push_back(i);

    assert(indices.size() == costs.size());

    for (int i = 0; i < indices.size(); ++i)
        (*node2cost)[indices[i]] = costs[i];
}


void compressed_weighted_converter_t::compute_hypothesis_cost(
    const pg::proof_graph_t *graph,
    hash_map<pg::node_idx_t, double> *node2cost) const
{
    for (int depth = 1;; ++depth)
    {
        const hash_set<pg::node_idx_t>
            *nodes = graph->search_nodes_with_depth(depth);
        if (nodes == NULL) break;

        hash_set<pg::hypernode_idx_t> hns;
        for (auto it = nodes->begin(); it != nodes->end(); ++it)
            hns.insert(graph->node(*it).master_hypernode());

        for (auto hn = hns.begin(); hn != hns.end(); ++hn)
        {
            pg::edge_idx_t parent = graph->find_parental_edge(*hn);
            if (parent < 0) continue;

            const pg::edge_t edge = graph->edge(parent);
            double cost_from(0.0);
            const std::vector<pg::node_idx_t>
                &hn_from = graph->hypernode(edge.tail()),
                &hn_to = graph->hypernode(edge.head());

            // COMPUTE SUM OF COST IN TAIL
            for (auto it = hn_from.begin(); it != hn_from.end(); ++it)
            {
                auto find = node2cost->find(*it);
                if (find != node2cost->end())
                    cost_from += find->second;
            }

            // ASSIGN COSTS TO HEAD NODES
            std::vector<double> weights((*m_weight_provider)(graph, parent));
            for (int i = 0; i < hn_to.size(); ++i)
                (*node2cost)[hn_to[i]] = weights[i] * cost_from;
        }
    }
}


void compressed_weighted_converter_t::assign_costs(
    const pg::proof_graph_t *graph, ilp::ilp_problem_t *prob,
    const hash_map<pg::node_idx_t, double> &node2cost) const
{
    /* TO BE FREED FROM THE COST,
     * THE NODE MUST SATISFY ONE OF FOLLOWING CONDITIONS:
     *   - ITS MASTER-HYPERNODE IS NOT TRUE.
     *   - ANY CHAINS FROM IT IS TRUE.
     *   - UNIFIES WITH ONE WHICH HAS LOWER COST. */

    auto hn2var = prob->hypernode_to_variable();
    hash_map<pg::node_idx_t, ilp::constraint_idx_t> node2cons;

    for (auto it = node2cost.begin(); it != node2cost.end(); ++it)
    {
        pg::node_idx_t idx_n = it->first;
        double cost = it->second;
        const pg::node_t &n = graph->node(idx_n);
        ilp::variable_idx_t cost_var = prob->add_variable(
            ilp::variable_t(format("cost:n(%d)", idx_n), cost));

        ilp::constraint_t cons(
            format("cost-condition:n(%d)", idx_n), ilp::OPR_GREATER_EQ, 0.0);
        cons.add_term(cost_var, 1.0);
        
        if (n.type() == pg::NODE_OBSERVABLE)
            cons.set_bound(1.0);
        else if (n.type() == pg::NODE_HYPOTHESIS)
            cons.add_term(hn2var.at(n.master_hypernode()), -1.0);

        node2cons[idx_n] = prob->add_constraint(cons);
    }

    for (pg::edge_idx_t i = 0; i < graph->edges().size(); ++i)
    {
        const pg::edge_t &e = graph->edge(i);
        auto tail = graph->hypernode(e.tail());

        if (e.is_chain_edge())
        for (auto n = tail.begin(); n != tail.end(); ++n)
        {
            ilp::constraint_idx_t con = node2cons.at(*n);
            ilp::variable_idx_t var = hn2var.at(e.head());
            prob->constraint(con).add_term(var, 1.0);
        }
        
        if (e.is_unify_edge())
        {
            ilp::variable_idx_t uni_v = prob->add_variable(
                ilp::variable_t(format("unify:e(%d)", i), 0.0));

            // TO UNIFY TAIL-NODES,
            // NODES IN TAIL AND HEAD-HYPERNODE MUST BE TRUE.
            ilp::constraint_t con_uni(
                format("condition_for_unify:e(%d)", i), ilp::OPR_GREATER_EQ, 0.0);
            pg::hypernode_idx_t mhn1 = graph->node(tail[0]).master_hypernode();
            pg::hypernode_idx_t mhn2 = graph->node(tail[0]).master_hypernode();
            if (mhn1 >= 0) con_uni.add_term(hn2var.at(mhn1), 1.0);
            if (mhn2 >= 0) con_uni.add_term(hn2var.at(mhn2), 1.0);
            if (e.head() >= 0)
                con_uni.add_term(hn2var.at(e.head()), 1.0);
            if (not con_uni.terms().empty())
                prob->add_constraint(con_uni);

            double cost1(node2cost.at(tail[0])), cost2(node2cost.at(tail[1]));
            ilp::constraint_idx_t con = node2cons.at(tail[cost1 > cost2 ? 0 : 1]);
            prob->constraint(con).add_term(uni_v, 1.0);
        }
    }
}



bool compressed_weighted_converter_t::
is_available(std::list<std::string> *message) const
{
    return true;
}


std::string compressed_weighted_converter_t::repr() const
{
    return "WeightedConverter";
}


compressed_weighted_converter_t::my_xml_decorator_t::
my_xml_decorator_t(const hash_map<pg::node_idx_t, double> &node2cost)
: m_node2cost(node2cost)
{}


void compressed_weighted_converter_t::my_xml_decorator_t::get_literal_attributes(
const ilp_solution_t *sol, pg::node_idx_t idx,
hash_map<std::string, std::string> *out) const
{
    auto find = m_node2cost.find(idx);
    if (find != m_node2cost.end())
        (*out)["cost"] = format("%lf", find->second);
}


bool compressed_weighted_converter_t::my_solution_interpreter_t::
node_is_active(const ilp_solution_t &sol, pg::node_idx_t idx) const
{
    const pg::node_t &node = sol.problem()->proof_graph()->node(idx);
    if (node.type() == pg::NODE_OBSERVABLE)
        return true;
    else if (node.is_equality_node() or node.is_non_equality_node())
    {
        ilp::variable_idx_t var = sol.problem()->find_variable_with_node(idx);
        return sol.variable_is_active(var);
    }
    else
    {
        pg::hypernode_idx_t hn = node.master_hypernode();
        ilp::variable_idx_t var =
            sol.problem()->find_variable_with_hypernode(hn);
        return sol.variable_is_active(var);
    }
}


bool compressed_weighted_converter_t::my_solution_interpreter_t::
hypernode_is_active(const ilp_solution_t &sol, pg::hypernode_idx_t idx) const
{
    ilp::variable_idx_t var =
        sol.problem()->find_variable_with_hypernode(idx);
    if (var >= 0)
        return sol.variable_is_active(var);
    else
    {
        const pg::proof_graph_t *graph = sol.problem()->proof_graph();
        auto hn = graph->hypernode(idx);
        hash_set<pg::hypernode_idx_t> considered;

        for (auto n = hn.begin(); n != hn.end(); ++n)
        {
            pg::hypernode_idx_t mhn = graph->node(*n).master_hypernode();
            if (mhn < 0 or considered.count(mhn) > 0) continue;

            ilp::variable_idx_t _v =
                sol.problem()->find_variable_with_hypernode(mhn);
            assert(_v >= 0);

            if (not sol.variable_is_active(_v))
                return false;
            else
                considered.insert(mhn);
        }

        return true;
    }
}


bool compressed_weighted_converter_t::my_solution_interpreter_t::
edge_is_active(const ilp_solution_t &sol, pg::edge_idx_t idx) const
{
    const pg::edge_t &edge = sol.problem()->proof_graph()->edge(idx);
    return
        hypernode_is_active(sol, edge.tail()) and
        hypernode_is_active(sol, edge.head());
}



}

}
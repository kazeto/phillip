#include "./ilp_converter.h"

namespace phil
{

namespace ilp
{


weighted_converter_t::weight_provider_t* weighted_converter_t::
parse_string_to_weight_provider(const std::string &str)
{
    if (not str.empty())
    {
        std::string pred;
        std::vector<std::string> terms;
        parse_string_as_function_call(str, &pred, &terms);

        if (pred == "fixed" and terms.size() == 1)
        {
            double weight;
            _sscanf(terms.at(0).c_str(), "%lf", &weight);
            return new fixed_weight_provider_t(weight);
        }

        if (pred == "basic" and terms.size() == 0)
        {
            return new basic_weight_provider_t();
        }

        print_error("The parameter for weight-provider is invalid: " + str);
    }

    return NULL;
}


weighted_converter_t::weighted_converter_t(
    double default_obs_cost, weight_provider_t *ptr)
: m_default_observation_cost(default_obs_cost), m_weight_provider(ptr)
{
    if (ptr == NULL)
        m_weight_provider = new basic_weight_provider_t();
}


weighted_converter_t::~weighted_converter_t()
{
    delete m_weight_provider;
}


ilp::ilp_problem_t* weighted_converter_t::execute() const
{
    const pg::proof_graph_t *graph = sys()->get_latent_hypotheses_set();
    ilp::ilp_problem_t *prob = new ilp::ilp_problem_t(
        graph, new ilp::basic_solution_interpreter_t(), false);

    // ADD VARIABLES FOR NODES
    for (pg::node_idx_t i = 0; i < graph->nodes().size(); ++i)
    {
        ilp::variable_idx_t var = prob->add_variable_of_node(i);
        if (graph->node(i).type() == pg::NODE_OBSERVABLE)
            prob->add_constancy_of_variable(var, 1.0);
    }

    // ADD VARIABLES FOR HYPERNODES
    for (pg::hypernode_idx_t i = 0; i < graph->hypernodes().size(); ++i)
        ilp::variable_idx_t var = prob->add_variable_of_hypernode(i);

    // ADD CONSTRAINTS FOR NODES
    for (pg::node_idx_t i = 0; i < graph->nodes().size(); ++i)
        prob->add_constraint_of_dependence_of_node_on_hypernode(i);

    // ADD CONSTRAINTS FOR HYPERNODES
    for (pg::hypernode_idx_t i = 0; i < graph->hypernodes().size(); ++i)
        prob->add_constraint_of_dependence_of_hypernode_on_parents(i);

    // ADD CONSTRAINTS FOR CHAINING EDGES
    for (pg::edge_idx_t i = 0; i < graph->edges().size(); ++i)
        prob->add_constrains_of_conditions_for_chain(i);

    prob->add_constrains_of_exclusive_chains();
    prob->add_constraints_of_transitive_unifications();

    // ASSIGN COSTS OF EDGES TO HYPERNODES
    hash_map<pg::node_idx_t, ilp::variable_idx_t> node2costvar;
    add_variables_for_observation_cost(graph, *sys()->get_input(), prob, &node2costvar);
    add_variables_for_hypothesis_cost(graph, prob, &node2costvar);
    add_constraints_for_cost(graph, prob, node2costvar);

    prob->add_xml_decorator(
        new my_xml_decorator_t(node2costvar));
    prob->add_attributes("converter", "weighted");

    return prob;
}


void weighted_converter_t::add_variable_for_cost(
    pg::node_idx_t idx, double cost, ilp::ilp_problem_t *prob,
    hash_map<pg::node_idx_t, ilp::variable_idx_t> *node2costvar) const
{
    ilp::variable_idx_t v(prob->find_variable_with_node(idx));
    if (v >= 0)
    {
        std::string name = format("cost(n:%d)", idx);
        ilp::variable_idx_t costvar =
            prob->add_variable(ilp::variable_t(name, cost));
        (*node2costvar)[idx] = costvar;
    }
}


void weighted_converter_t::add_variables_for_observation_cost(
    const pg::proof_graph_t *graph,
    const lf::input_t &input, ilp::ilp_problem_t *prob,
    hash_map<pg::node_idx_t, ilp::variable_idx_t> *node2costvar) const
{
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
        add_variable_for_cost(indices[i], costs[i], prob, node2costvar);
}


void weighted_converter_t::add_variables_for_hypothesis_cost(
    const pg::proof_graph_t *graph, ilp::ilp_problem_t *prob,
    hash_map<pg::node_idx_t, ilp::variable_idx_t> *node2costvar) const
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
                &hn_from = graph->hypernode(edge.tail());

            // COMPUTE SUM OF COST IN TAIL
            for (auto it = hn_from.begin(); it != hn_from.end(); ++it)
            {
                auto find = node2costvar->find(*it);
                if (find != node2costvar->end())
                    cost_from += prob->variable(find->second).objective_coefficient();
            }

            const std::vector<pg::node_idx_t>
                &hn_to = graph->hypernode(edge.head());
            std::vector<double> weights((*m_weight_provider)(graph, parent));

            // ASSIGN COSTS TO HEAD NODES
            for (int i = 0; i < hn_to.size(); ++i)
                add_variable_for_cost(hn_to[i], weights[i] * cost_from, prob, node2costvar);
        }
    }
}


void weighted_converter_t::add_constraints_for_cost(
    const pg::proof_graph_t *graph, ilp::ilp_problem_t *prob,
    const hash_map<pg::node_idx_t, ilp::variable_idx_t> &node2costvar) const
{
    for (auto it = node2costvar.begin(); it != node2costvar.end(); ++it)
    {
        pg::node_idx_t n_idx = it->first;
        ilp::variable_idx_t nodevar = prob->find_variable_with_node(n_idx);
        ilp::variable_idx_t costvar = it->second;

        // IF THE TARGET NODE IS HYPOTHESIZE,
        // ONE OF FOLLOWING CONDITIONS MUST BE SATISFIED:
        //   - ITS COST HAS BEEN PAID.
        //   - ONE OF ITS CHILDLEN NODES HAS BEEN HYPOTHESIZED.
        //   - IT HAS BEEN UNIFIED WITH A NODE WHOSE COST IS LESS THAN IT.

        ilp::constraint_t cons(
            format("cost-payment(n:%d)", n_idx), ilp::OPR_GREATER_EQ, 0.0);
        cons.add_term(nodevar, -1.0);
        cons.add_term(costvar, 1.0);

        hash_set<pg::edge_idx_t> edges;
        const hash_set<pg::hypernode_idx_t> *hns =
            graph->search_hypernodes_with_node(n_idx);

        if (hns != NULL)
        for (auto hn = hns->begin(); hn != hns->end(); ++hn)
        {
            const hash_set<pg::edge_idx_t>
                *es = graph->search_edges_with_hypernode(*hn);
            if (es == NULL) continue;

            for (auto e = es->begin(); e != es->end(); ++e)
            {
                const pg::edge_t edge = graph->edge(*e);
                if (edge.tail() != *hn) continue;

                if (edge.is_chain_edge())
                    edges.insert(*e);
                else if (edge.is_unify_edge())
                {
                    auto from = graph->hypernode(edge.tail());
                    double cost1 = get_cost(from[0], prob, node2costvar);
                    double cost2 = get_cost(from[1], prob, node2costvar);
                    if ((n_idx == from[0]) == (cost1 > cost2))
                        edges.insert(*e);
                }
            }
        }

        for (auto e = edges.begin(); e != edges.end(); ++e)
        {
            pg::hypernode_idx_t head = graph->edge(*e).head();
            ilp::variable_idx_t var = prob->find_variable_with_hypernode(head);
            if (var >= 0)
                cons.add_term(var, 1.0);
        }

        prob->add_constraint(cons);
    }
}


double weighted_converter_t::get_cost(
    pg::node_idx_t idx, const ilp::ilp_problem_t *prob,
    const hash_map<pg::node_idx_t, ilp::variable_idx_t> &node2costvar) const
{
    auto find = node2costvar.find(idx);
    return (find != node2costvar.end()) ?
        prob->variable(find->second).objective_coefficient() : 0.0;
}


bool weighted_converter_t::is_available(std::list<std::string> *message) const
{
    return true;
}


std::string weighted_converter_t::repr() const
{
    return "WeightedConverter";
}


std::vector<double> weighted_converter_t::fixed_weight_provider_t::operator()(
    const pg::proof_graph_t *graph, pg::edge_idx_t idx) const
{
    const pg::edge_t &edge = graph->edge(idx);
    size_t size = graph->hypernode(edge.head()).size();
    return std::vector<double>(size, m_weight / double(size));
}


std::vector<double> weighted_converter_t::basic_weight_provider_t::operator()(
    const pg::proof_graph_t *graph, pg::edge_idx_t idx) const
{
    const kb::knowledge_base_t *base = sys()->knowledge_base();
    const pg::edge_t &edge = graph->edge(idx);
    std::vector<double> weights(graph->hypernode(edge.head()).size(), 1.0);

    if (edge.is_chain_edge())
    {
        lf::axiom_t axiom(base->get_axiom(edge.axiom_id()));
        lf::logical_function_t branch =
            axiom.func.branch(edge.type() == pg::EDGE_HYPOTHESIZE ? 0 : 1);

        if (weights.size() == 1 and branch.is_operator(lf::OPR_LITERAL))
            branch.param2double(&weights[0]);
        else
        {
            for (int i = 0; i < weights.size(); ++i)
                branch.branch(i).param2double(&weights[i]);
        }
    }

    return weights;
}


weighted_converter_t::my_xml_decorator_t::
my_xml_decorator_t(
const hash_map<pg::node_idx_t, ilp::variable_idx_t> &node2costvar)
: m_node2costvar(node2costvar)
{}


void weighted_converter_t::my_xml_decorator_t::
get_literal_attributes(
const ilp_solution_t *sol, pg::node_idx_t idx,
hash_map<std::string, std::string> *out) const
{
    auto find = m_node2costvar.find(idx);
    if (find != m_node2costvar.end())
    {
        variable_idx_t costvar = find->second;
        double cost(sol->problem()->variable(costvar).objective_coefficient());
        (*out)["cost"] = format("%lf", cost);
        (*out)["paid-cost"] = sol->variable_is_active(costvar) ? "yes" : "no";
    }
}




}

}
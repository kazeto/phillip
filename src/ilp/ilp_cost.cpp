#include "./ilp_converter.h"

namespace phil
{

namespace ilp
{


costed_converter_t::costed_converter_t(cost_provider_t *ptr)
: m_cost_provider(ptr)
{
    if (m_cost_provider == NULL)
        m_cost_provider = new basic_cost_provider_t();
}


costed_converter_t::~costed_converter_t()
{
    delete m_cost_provider;
}


ilp::ilp_problem_t* costed_converter_t::execute() const
{
    const pg::proof_graph_t *graph = sys()->get_latent_hypotheses_set();
    ilp::ilp_problem_t *prob = new ilp::ilp_problem_t(
        graph, new ilp::basic_solution_interpreter_t(), true);

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

    // ASSIGN COSTS OF EDGES TO HYPERNODES
    for (pg::edge_idx_t i = 0; i < graph->edges().size(); ++i)
    {
        ilp::variable_idx_t var =
            prob->find_variable_with_hypernode(graph->edge(i).head());
        if (var >= 0)
        {
            double cost = (*m_cost_provider)(graph, i);
            prob->variable(var).set_coefficient(cost);
        }
    }

    prob->add_constrains_of_exclusive_chains();
    prob->add_constraints_of_transitive_unifications();

    return prob;
}


bool costed_converter_t::is_available(std::list<std::string> *message) const
{
    return true;
}


std::string costed_converter_t::repr() const
{
    return "CostedConverter";
}


double costed_converter_t::basic_cost_provider_t::operator()(
    const pg::proof_graph_t *graph, pg::edge_idx_t idx) const
{
    const kb::knowledge_base_t *base = sys()->knowledge_base();
    const pg::edge_t &edge = graph->edge(idx);
    double cost(0.0);

    if (edge.type() == pg::EDGE_HYPOTHESIZE or
        edge.type() == pg::EDGE_IMPLICATION)
    {
        lf::axiom_t axiom(base->get_axiom(edge.axiom_id()));
        auto splitted = split(axiom.func.param(), ":");

        for (auto it = splitted.begin(); it != splitted.end(); ++it)
        if (_sscanf(it->c_str(), "%lf", &cost) == 1)
            break;
    }

    return cost;
}


}

}
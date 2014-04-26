#include <regex>
#include "./ilp_converter.h"

namespace phil
{

namespace ilp
{


costed_converter_t::cost_provider_t* costed_converter_t::
parse_string_to_cost_provider(const std::string &str)
{
    if (not str.empty())
    {
        std::regex pattern(
            "^basic\\(\\s*"
            "([+-]?\\d*[\\.]?\\d+),\\s*"
            "([+-]?\\d*[\\.]?\\d+),\\s*"
            "([+-]?\\d*[\\.]?\\d+)\\s*"
            "\\)\\s*$");
        std::smatch sm;
        if (std::regex_match(str, sm, pattern))
        {
            float def_cost, lit_unif_cost, term_unif_cost;
            _sscanf(sm[1].str().c_str(), "%f", &def_cost);
            _sscanf(sm[2].str().c_str(), "%f", &lit_unif_cost);
            _sscanf(sm[3].str().c_str(), "%f", &term_unif_cost);

            return new basic_cost_provider_t(
                def_cost, lit_unif_cost, term_unif_cost);
        }

        print_error("The parameter for cost-provider is invalid: " + str);
    }

    return NULL;
}


costed_converter_t::costed_converter_t(cost_provider_t *ptr)
: m_cost_provider(ptr)
{
    if (m_cost_provider == NULL)
        m_cost_provider = new basic_cost_provider_t(10.0, -40.0, 2.0);
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

    // ASSIGN COSTS OF NODES
    for (pg::node_idx_t i = 0; i < graph->nodes().size(); ++i)
    {
        ilp::variable_idx_t var =
            prob->find_variable_with_node(i);
        if (var >= 0)
        {
            double cost = m_cost_provider->node_cost(graph, i);
            prob->variable(var).set_coefficient(cost);
        }
    }

    // ASSIGN COSTS OF EDGES TO HEAD-HYPERNODES
    for (pg::edge_idx_t i = 0; i < graph->edges().size(); ++i)
    {
        ilp::variable_idx_t var =
            prob->find_variable_with_hypernode(graph->edge(i).head());
        if (var >= 0)
        {
            double cost = m_cost_provider->edge_cost(graph, i);
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


costed_converter_t::basic_cost_provider_t::basic_cost_provider_t(
    double default_cost, double literal_unify_cost, double term_unify_cost)
    : m_default_axiom_cost(default_cost),
      m_literal_unifying_cost(literal_unify_cost),
      m_term_unifying_cost(term_unify_cost)
{}


double costed_converter_t::basic_cost_provider_t::edge_cost(
    const pg::proof_graph_t *graph, pg::edge_idx_t idx) const
{
    const kb::knowledge_base_t *base = sys()->knowledge_base();
    const pg::edge_t &edge = graph->edge(idx);
    double cost(0.0);

    if (edge.is_chain_edge())
    {
        lf::axiom_t axiom(base->get_axiom(edge.axiom_id()));
        auto splitted = split(axiom.func.param(), ":");

        for (auto it = splitted.begin(); it != splitted.end(); ++it)
        if (_sscanf(it->c_str(), "%lf", &cost) == 1)
            break;
    }
    else if (edge.is_unify_edge())
        cost = m_literal_unifying_cost;

    return cost;
}


double costed_converter_t::basic_cost_provider_t::node_cost(
    const pg::proof_graph_t *graph, pg::node_idx_t idx) const
{
    if (m_term_unifying_cost != 0.0)
    if (graph->node(idx).is_equality_node())
        m_term_unifying_cost;

    return 0.0;
}



}

}
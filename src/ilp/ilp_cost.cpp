#include "./ilp_converter.h"

namespace phil
{

namespace cnv
{


costed_converter_t::cost_provider_t* costed_converter_t::
parse_string_to_cost_provider(const std::string &str)
{
    if (not str.empty())
    {
        std::string pred;
        std::vector<std::string> terms;
        parse_string_as_function_call(str, &pred, &terms);

        if (pred == "basic" and terms.size() == 3)
        {
            float def_cost, lit_unif_cost, term_unif_cost;
            _sscanf(terms.at(0).c_str(), "%f", &def_cost);
            _sscanf(terms.at(1).c_str(), "%f", &lit_unif_cost);
            _sscanf(terms.at(2).c_str(), "%f", &term_unif_cost);

            return new basic_cost_provider_t(
                def_cost, lit_unif_cost, term_unif_cost);
        }
        else
            throw phillip_exception_t(
            "The parameter for cost-provider is invalid: " + str);
    }

    return NULL;
}


costed_converter_t::costed_converter_t(phillip_main_t *main, cost_provider_t *ptr)
: ilp_converter_t(main), m_cost_provider(ptr)
{
    if (m_cost_provider == NULL)
        m_cost_provider = new basic_cost_provider_t(10.0, -40.0, 2.0);
}


costed_converter_t::~costed_converter_t()
{
    delete m_cost_provider;
}


ilp_converter_t* costed_converter_t::duplicate(phillip_main_t *ptr) const
{
    return new costed_converter_t(ptr, m_cost_provider->duplicate());
}


ilp::ilp_problem_t* costed_converter_t::execute() const
{
    auto begin = std::chrono::system_clock::now();

    const pg::proof_graph_t *graph = phillip()->get_latent_hypotheses_set();
    ilp::ilp_problem_t *prob = new ilp::ilp_problem_t(
        graph, new ilp::basic_solution_interpreter_t(), false);

    convert_proof_graph(prob);
    if (prob->is_timeout()) return prob;

#define _check_timeout if(do_time_out(begin)) { prob->timeout(true); return prob; }

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
    _check_timeout;

    // ASSIGN COSTS OF EDGES
    for (pg::edge_idx_t i = 0; i < graph->edges().size(); ++i)
    {
        ilp::variable_idx_t var =
            prob->find_variable_with_edge(i);
        if (var >= 0)
        {
            double cost = m_cost_provider->edge_cost(graph, i);
            prob->variable(var).set_coefficient(cost);
        }
    }
    _check_timeout;

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


ilp_converter_t* costed_converter_t::
generator_t::operator()(phillip_main_t *ph) const
{
    const std::string &param = ph->param("cost_provider");
    costed_converter_t::cost_provider_t *ptr =
        costed_converter_t::parse_string_to_cost_provider(param);

    return new costed_converter_t(ph, ptr);
}


costed_converter_t::basic_cost_provider_t::basic_cost_provider_t(
    double default_cost, double literal_unify_cost, double term_unify_cost)
    : m_default_axiom_cost(default_cost),
      m_literal_unifying_cost(literal_unify_cost),
      m_term_unifying_cost(term_unify_cost)
{}


costed_converter_t::cost_provider_t*
costed_converter_t::basic_cost_provider_t::duplicate() const
{
    return new basic_cost_provider_t(
        m_default_axiom_cost, m_literal_unifying_cost, m_term_unifying_cost);
}


double costed_converter_t::basic_cost_provider_t::edge_cost(
    const pg::proof_graph_t *graph, pg::edge_idx_t idx) const
{
    const kb::knowledge_base_t *base = kb::knowledge_base_t::instance();
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
    {
        const std::vector<pg::node_idx_t>& from =
            graph->hypernode(graph->edge(idx).tail());
        if (graph->node(from[0]).type() != pg::NODE_REQUIRED and
            graph->node(from[1]).type() != pg::NODE_REQUIRED)
            cost = m_literal_unifying_cost;
    }

    return cost;
}


double costed_converter_t::basic_cost_provider_t::node_cost(
    const pg::proof_graph_t *graph, pg::node_idx_t idx) const
{
    if (graph->node(idx).is_equality_node())
        return m_term_unifying_cost;

    return 0.0;
}



}

}

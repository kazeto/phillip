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

        if (pred == "basic")
        {
            double weight(1.2);
            if (terms.size() >= 1)
                _sscanf(terms.at(0).c_str(), "%lf", &weight);
            return new basic_weight_provider_t(weight);
        }

        print_error("The parameter for weight-provider is invalid: " + str);
    }

    return NULL;
}


weighted_converter_t::weighted_converter_t(
    phillip_main_t *main, double default_obs_cost, weight_provider_t *ptr)
    : ilp_converter_t(main), m_default_observation_cost(default_obs_cost),
      m_weight_provider(ptr)
{
    if (ptr == NULL)
        m_weight_provider = new basic_weight_provider_t(1.2);
}


weighted_converter_t::~weighted_converter_t()
{
    delete m_weight_provider;
}


ilp_converter_t* weighted_converter_t::duplicate(phillip_main_t *ptr) const
{
    return new weighted_converter_t(
        ptr, m_default_observation_cost, m_weight_provider->duplicate());
}


ilp::ilp_problem_t* weighted_converter_t::execute() const
{
    std::time_t begin;
    std::time(&begin);
    
    const pg::proof_graph_t *graph = phillip()->get_latent_hypotheses_set();
    ilp::ilp_problem_t *prob = new ilp::ilp_problem_t(
        graph, new ilp::basic_solution_interpreter_t(), false,
        graph->name());

#define _check_timeout if(is_timeout(begin)) { prob->timeout(true); return prob; }
    
    // ADD VARIABLES FOR NODES
    for (pg::node_idx_t i = 0; i < graph->nodes().size(); ++i)
    {
        ilp::variable_idx_t var = prob->add_variable_of_node(i);
        if (graph->node(i).type() == pg::NODE_OBSERVABLE)
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

    if (phillip()->flag("pseudo_positive"))
    {
        const lf::logical_function_t *req = phillip()->get_requirement();
        if (req != NULL)
            prob->add_variable_for_requirement(*req, false);
        _check_timeout;
    }
    
    prob->add_constraints_of_mutual_exclusions();
    _check_timeout;
    
    prob->add_constrains_of_exclusive_chains();
    _check_timeout;
    
    prob->add_constraints_of_transitive_unifications();
    _check_timeout;

    // ASSIGN COSTS OF EDGES TO HYPERNODES
    hash_map<pg::node_idx_t, ilp::variable_idx_t> node2costvar;

    auto add_variable_for_cost = [&, this](pg::node_idx_t idx, double cost)
    {
        ilp::variable_idx_t v(prob->find_variable_with_node(idx));
        if (v >= 0)
        {
            std::string name = format("cost(n:%d)", idx);
            ilp::variable_idx_t costvar =
                prob->add_variable(ilp::variable_t(name, cost));
            node2costvar[idx] = costvar;
        }
    };

    auto add_variables_for_observation_cost = [&, this](const lf::input_t &input)
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
            add_variable_for_cost(indices[i], costs[i]);
    };

    auto add_variables_for_hypothesis_cost = [&, this]()
    {
        for (int depth = 1; not is_timeout(begin); ++depth)
        {
            const hash_set<pg::node_idx_t>
            *nodes = graph->search_nodes_with_depth(depth);
            if (nodes == NULL) break;

            hash_set<pg::hypernode_idx_t> hns;
            for (auto it = nodes->begin(); it != nodes->end(); ++it)
                hns.insert(graph->node(*it).master_hypernode());

            for (auto hn : hns)
            {
                pg::edge_idx_t parent = graph->find_parental_edge(hn);
                if (parent < 0) continue;

                const pg::edge_t edge = graph->edge(parent);
                double cost_from(0.0);
                const std::vector<pg::node_idx_t>
                    &hn_from = graph->hypernode(edge.tail());

                // COMPUTE SUM OF COST IN TAIL
                for (auto it = hn_from.begin(); it != hn_from.end(); ++it)
                {
                    auto find = node2costvar.find(*it);
                    if (find != node2costvar.end())
                        cost_from += prob->variable(find->second).objective_coefficient();
                }

                const std::vector<pg::node_idx_t>
                    &hn_to = graph->hypernode(edge.head());
                std::vector<double> weights((*m_weight_provider)(graph, parent));

                // ASSIGN COSTS TO HEAD NODES
                for (int i = 0; i < hn_to.size(); ++i)
                    add_variable_for_cost(hn_to[i], weights[i] * cost_from);

                if (is_timeout(begin)) break;
            }            
        }
    };

    auto add_constraints_for_cost = [&, this]()
    {
        auto get_cost_of_node = [&](pg::node_idx_t idx) -> double
        {
            auto find = node2costvar.find(idx);
            if (find != node2costvar.end())
                return prob->variable(find->second).objective_coefficient();
            else
                return 0.0;
        };
        
        for (auto p : node2costvar)
        {
            if (is_timeout(begin)) break;
            
            pg::node_idx_t n_idx = p.first;
            ilp::variable_idx_t nodevar = prob->find_variable_with_node(n_idx);
            ilp::variable_idx_t costvar = p.second;

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
            for (auto hn : (*hns))
            {
                const hash_set<pg::edge_idx_t>
                    *es = graph->search_edges_with_hypernode(hn);
                if (es == NULL) continue;

                for (auto e : (*es))
                {
                    const pg::edge_t edge = graph->edge(e);

                    // TARGETS ONLY EDGES WHOSE HEAD INCLUDES n_idx
                    if (edge.tail() != hn) continue;

                    if (edge.is_chain_edge())
                        edges.insert(e);
                    else if (edge.is_unify_edge())
                    {
                        auto from = graph->hypernode(edge.tail());
                        double cost1 = get_cost_of_node(from[0]);
                        double cost2 = get_cost_of_node(from[1]);
                        if ((n_idx == from[0]) == (cost1 > cost2))
                            edges.insert(e);
                    }
                }

                if (is_timeout(begin)) break;
            }

            for (auto e : edges)
            {
                ilp::variable_idx_t var = prob->find_variable_with_edge(e);
                if (var >= 0) cons.add_term(var, 1.0);
                if (is_timeout(begin)) break;
            }

            prob->add_constraint(cons);            
        }

        for (pg::edge_idx_t i = 0; i < graph->edges().size(); ++i)
        {
            const pg::edge_t &e_uni = graph->edge(i);
            if (not e_uni.is_unify_edge()) continue;

            // IF A LITERAL IS UNIFIED AND EXCUSED FROM PAYING COST,
            // CHAINING FROM THE LITERAL IS FORBIDDEN.

            ilp::variable_idx_t v_uni_tail = prob->find_variable_with_hypernode(e_uni.tail());
            ilp::variable_idx_t v_uni_head = prob->find_variable_with_hypernode(e_uni.head());
            ilp::variable_idx_t v_uni = (e_uni.head() >= 0 ? v_uni_head : v_uni_tail);
            if (v_uni_tail < 0 or(e_uni.head() >= 0 and v_uni_head < 0)) continue;

            auto from = graph->hypernode(e_uni.tail());
            double cost1 = get_cost_of_node(from[0]);
            double cost2 = get_cost_of_node(from[1]);
            pg::node_idx_t
            explained((cost1 > cost2) ? from[0] : from[1]),
            explains((cost1 > cost2) ? from[1] : from[0]);

            prob->add_constraints_to_forbid_chaining_from_explained_node(i, explained);
            prob->add_constraints_to_forbid_looping_unification(i, explained);

            if (is_timeout(begin)) break;
        }
    };
    
    add_variables_for_observation_cost(*phillip()->get_input());
    _check_timeout;
    
    add_variables_for_hypothesis_cost();
    _check_timeout;
    
    add_constraints_for_cost();
    _check_timeout;

    prob->add_xml_decorator(
        new my_xml_decorator_t(node2costvar));
    prob->add_attributes("converter", "weighted");

#undef _check_timeout
    
    return prob;
}


bool weighted_converter_t::is_timeout(std::time_t begin) const
{
    std::time_t now;
    std::time(&now);
    int t_ilp(now - begin);
    int t_all(phillip()->get_time_for_lhs() + t_ilp);
    
    return phillip()->is_timeout_ilp(t_ilp) or phillip()->is_timeout_all(t_all);
}


bool weighted_converter_t::is_available(std::list<std::string> *message) const
{
    return true;
}


std::string weighted_converter_t::repr() const
{
    return "WeightedConverter";
}


ilp_converter_t::enumeration_stopper_t*
weighted_converter_t::enumeration_stopper() const
{
    return new my_enumeration_stopper_t(this);
}


std::vector<double> weighted_converter_t::basic_weight_provider_t::operator()(
    const pg::proof_graph_t *graph, pg::edge_idx_t idx) const
{
    const kb::knowledge_base_t *base = kb::knowledge_base_t::instance();
    const pg::edge_t &edge = graph->edge(idx);
    size_t size = graph->hypernode(edge.head()).size();
    std::vector<double> weights(size, 0.0);
    bool do_use_default(true);

    if (edge.is_chain_edge())
    {
        lf::axiom_t axiom(base->get_axiom(edge.axiom_id()));
        lf::logical_function_t branch =
            axiom.func.branch(edge.type() == pg::EDGE_HYPOTHESIZE ? 0 : 1);

        if (weights.size() == 1 and branch.is_operator(lf::OPR_LITERAL))
        {
            if (branch.param2double(&weights[0]))
                do_use_default = false;
        }
        else
        {
            for (int i = 0; i < weights.size(); ++i)
            {
                if (branch.branch(i).param2double(&weights[i]))
                    do_use_default = false;
                else
                    weights[i] = 0.0;
            }
        }
    }

    if (do_use_default)
        weights.assign(size, m_default_weight / (double)size);

    return weights;
}


weighted_converter_t::weight_provider_t*
weighted_converter_t::basic_weight_provider_t::duplicate() const
{
    return new basic_weight_provider_t(m_default_weight);
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



bool weighted_converter_t::
my_enumeration_stopper_t::operator()(const pg::proof_graph_t *graph)
{
    pg::edge_idx_t idx(-1);
    for (pg::edge_idx_t i = graph->edges().size() - 1; i >= 0; --i)
    if (graph->edge(i).is_unify_edge())
    {
        idx = i;
        break;
    }

    if (idx < 0) return false;
    if (m_considered_edges.count(idx) > 0) return false;

    m_considered_edges.insert(idx);
    return true;
}



}

}

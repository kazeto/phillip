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
    double default_obs_cost, weight_provider_t *ptr)
: m_default_observation_cost(default_obs_cost), m_weight_provider(ptr)
{
    if (ptr == NULL)
        m_weight_provider = new basic_weight_provider_t(1.2);
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

                // TARGETS ONLY EDGES WHOSE HEAD INCLUDES n_idx
                if (edge.tail() != *hn) continue;

                if (edge.is_chain_edge())
                    edges.insert(*e);
                else if (edge.is_unify_edge())
                {
                    auto from = graph->hypernode(edge.tail());
                    double cost1 = get_cost_of_node(from[0], prob, node2costvar);
                    double cost2 = get_cost_of_node(from[1], prob, node2costvar);
                    if ((n_idx == from[0]) == (cost1 > cost2))
                        edges.insert(*e);
                }
            }
        }

        for (auto e = edges.begin(); e != edges.end(); ++e)
        {
            pg::hypernode_idx_t head = graph->edge(*e).head();
            ilp::variable_idx_t var = prob->find_variable_with_hypernode(
                head >= 0 ? head : graph->edge(*e).tail());

            if (var >= 0)
                cons.add_term(var, 1.0);
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
        double cost1 = get_cost_of_node(from[0], prob, node2costvar);
        double cost2 = get_cost_of_node(from[1], prob, node2costvar);
        pg::node_idx_t
            explained((cost1 > cost2) ? from[0] : from[1]),
            explains((cost1 > cost2) ? from[1] : from[0]);

        auto hns = graph->search_hypernodes_with_node(explained);
        for (auto hn = hns->begin(); hn != hns->end(); ++hn)
        {
            auto es = graph->search_edges_with_hypernode(*hn);
            for (auto j = es->begin(); j != es->end(); ++j)
            {
                const pg::edge_t &e_ch = graph->edge(*j);
                if (not e_ch.is_chain_edge()) continue;
                if (e_ch.tail() != (*hn)) continue;

                ilp::constraint_t con(
                    format("unify_or_chain:e(%d):e(%d)", i, *j),
                    ilp::OPR_GREATER_EQ, -1.0);
                ilp::variable_idx_t v_ch_head =
                    prob->find_variable_with_hypernode(e_ch.head());
                if (v_ch_head >= 0)
                {
                    con.add_term(v_ch_head, -1.0);
                    con.add_term(v_uni_tail, -1.0);
                    if (e_uni.head() >= 0)
                    {
                        con.add_term(v_uni_head, -1.0);
                        con.set_bound(con.bound() - 1.0);
                    }
                    prob->add_constraint(con);
                }
            }
        }

        // IF LITERAL p & q IS UNIFIED AND COST(p) < COST(q), 
        // A LITERAL HYPOTHESIZED FROM p CANNOT UNIFY WITH
        // A LITERAL IN EVIDENCES OF q.

        hash_set<pg::node_idx_t> descendants;
        hash_set<pg::node_idx_t> ancestors(graph->node(explained).evidences());
        
        graph->enumerate_descendant_nodes(explains, &descendants);
        descendants.insert(explains);
        ancestors.insert(explained);
        
        hash_map<std::string, hash_set<pg::node_idx_t> > a2n_1, a2n_2;
        for (auto it = descendants.begin(); it != descendants.end(); ++it)
            a2n_1[graph->node(*it).literal().get_predicate_arity()].insert(*it);
        for (auto it = ancestors.begin(); it != ancestors.end(); ++it)
            a2n_2[graph->node(*it).literal().get_predicate_arity()].insert(*it);

        for (auto it1 = a2n_1.begin(); it1 != a2n_1.end(); ++it1)
        {
            auto it2 = a2n_2.find(it1->first);
            if (it2 == a2n_2.end()) continue;

            for (auto n1 = it1->second.begin(); n1 != it1->second.end(); ++n1)
            for (auto n2 = it2->second.begin(); n2 != it2->second.end(); ++n2)
            {
                pg::edge_idx_t _uni = graph->find_unifying_edge(*n1, *n2);
                if (_uni < 0 or _uni == i) continue;

                const pg::edge_t &_e_uni = graph->edge(_uni);
                ilp::variable_idx_t _v_uni_tail = prob->find_variable_with_hypernode(_e_uni.tail());
                ilp::variable_idx_t _v_uni_head = prob->find_variable_with_hypernode(_e_uni.head());

                if (_v_uni_tail >= 0 and (_e_uni.head() < 0 or _v_uni_head >= 0))
                {
                    ilp::constraint_t con(
                        format("muex_unify:e(%d,%d)", i, _uni),
                        ilp::OPR_GREATER_EQ, -1.0);

                    con.add_term(v_uni_tail, -1.0);
                    con.add_term(_v_uni_tail, -1.0);
                    if (e_uni.head() >= 0)
                    {
                        con.add_term(v_uni_head, -1.0);
                        con.set_bound(con.bound() - 1.0);
                    }
                    if (_e_uni.head() >= 0 and _e_uni.head() != e_uni.head())
                    {
                        con.add_term(_v_uni_head, -1.0);
                        con.set_bound(con.bound() - 1.0);
                    }

                    prob->add_constraint(con);
                }
            }
        }
    }
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
    const kb::knowledge_base_t *base = sys()->knowledge_base();
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

#include <random>
#include "./ilp_converter.h"

namespace phil
{

namespace cnv
{


weighted_converter_t::cost_provider_t* weighted_converter_t::
generate_cost_provider(const phillip_main_t *ph)
{
    cost_provider_t *out = NULL;
    const std::string &key = ph->param("cost-provider");
    double def_weight = ph->param_float("default-axiom-weight", 1.2);
    double def_cost = ph->param_float("default-observation-cost", 10.0);

    if (not key.empty())
    {
        if (key == "basic")
            return new basic_cost_provider_t(
            std::multiplies<double>(), def_cost, def_weight, "multiply");

        if (key == "linear")
            return new basic_cost_provider_t(
            std::plus<double>(), def_cost, def_weight, "addition");

        throw phillip_exception_t(
            "The parameter for weight-provider is invalid: " + key);
    }

    // DEFAULT
    return new basic_cost_provider_t(
        std::multiplies<double>(), def_cost, def_weight, "multiply");
}


weighted_converter_t::weighted_converter_t(
    const phillip_main_t *main, cost_provider_t *ptr)
    : ilp_converter_t(main), m_cost_provider(ptr)
{}


ilp::ilp_problem_t* weighted_converter_t::execute() const
{
    auto begin = std::chrono::system_clock::now();
    
    const pg::proof_graph_t *graph = phillip()->get_latent_hypotheses_set();
    ilp::ilp_problem_t *prob = new ilp::ilp_problem_t(
        graph, new ilp::basic_solution_interpreter_t(), false);

    convert_proof_graph(prob);
    if (prob->has_timed_out()) return prob;

#define _check_timeout if(do_time_out(begin)) { prob->timeout(true); return prob; }
    
    // HYPOTHESIS COSTS ASSIGNED EACH NODE
    hash_map<pg::node_idx_t, ilp::variable_idx_t> node2costvar;

    auto add_variable_for_cost = [&, this](pg::node_idx_t idx, double cost)
    {
        ilp::variable_idx_t v(prob->find_variable_with_node(idx));
        if (v >= 0)
        {
            std::string name = util::format("cost(n:%d)", idx);
            ilp::variable_idx_t costvar =
                prob->add_variable(ilp::variable_t(name, cost));
            node2costvar[idx] = costvar;
        }
    };
    
    for (auto p : (*m_cost_provider)(graph))
    {
        add_variable_for_cost(p.first, p.second);
        if (do_time_out(begin)) break;
    }

    auto get_cost_of_node = [&](pg::node_idx_t idx) -> double
    {
        auto find = node2costvar.find(idx);        
        return (find != node2costvar.end()) ?
            prob->variable(find->second).objective_coefficient() : 0.0;
    };
        
    for (auto p : node2costvar)
    {
        if (do_time_out(begin)) break;
            
        pg::node_idx_t n_idx = p.first;
        ilp::variable_idx_t nodevar = prob->find_variable_with_node(n_idx);
        ilp::variable_idx_t costvar = p.second;

        // IF THE TARGET NODE IS HYPOTHESIZE,
        // ONE OF FOLLOWING CONDITIONS MUST BE SATISFIED:
        //   - ITS COST HAS BEEN PAID.
        //   - ONE OF ITS CHILDLEN NODES HAS BEEN HYPOTHESIZED.
        //   - IT HAS BEEN UNIFIED WITH A NODE WHOSE COST IS LESS THAN IT AND IS NOT A REQUIREMENT.

        ilp::constraint_t cons(
            util::format("cost-payment(n:%d)", n_idx),
            ilp::OPR_GREATER_EQ, 0.0);
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

                // ONLY EDGES WHOSE HEAD INCLUDES n_idx ARE APPLICABLE.
                if (edge.tail() != hn) continue;

                if (edge.is_chain_edge())
                    edges.insert(e);
                else if (edge.is_unify_edge())
                {
                    auto from = graph->hypernode(edge.tail());

                    if (graph->node(from[0]).type() == pg::NODE_REQUIRED or
                        graph->node(from[1]).type() == pg::NODE_REQUIRED)
                        continue;

                    double cost1 = get_cost_of_node(from[0]);
                    double cost2 = get_cost_of_node(from[1]);
                    if ((n_idx == from[0]) == (cost1 > cost2))
                        edges.insert(e);
                }
            }

            if (do_time_out(begin)) break;
        }

        for (auto e : edges)
        {
            ilp::variable_idx_t var = prob->find_variable_with_edge(e);
            if (var >= 0) cons.add_term(var, 1.0);
            if (do_time_out(begin)) break;
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

        if (do_time_out(begin)) break;
    }
    
    prob->add_xml_decorator(new xml_decorator_t(node2costvar));
    prob->add_attributes("converter", repr());

#undef _check_timeout
    
    return prob;
}


bool weighted_converter_t::is_available(std::list<std::string> *message) const
{
    return true;
}


std::string weighted_converter_t::repr() const
{
    return "weighted-converter(" + m_cost_provider->repr() + ")";
}


ilp_converter_t* weighted_converter_t::
generator_t::operator()(const phillip_main_t *ph) const
{
    return new weighted_converter_t(
        ph, weighted_converter_t::generate_cost_provider(ph));
}


/* -------- Methods of cost_provider_t -------- */


void weighted_converter_t::cost_provider_t::
get_observation_costs(const pg::proof_graph_t *g, double default_cost, node2cost_map_t *out)
{
    const lf::input_t &input = *(g->phillip()->get_input());
    assert(input.obs.is_operator(lf::OPR_AND));

    const std::vector<lf::logical_function_t> &obs = input.obs.branches();
    std::vector<double> costs;
    std::vector<pg::node_idx_t> indices;

    for (auto obs : input.obs.branches())
    {
        double cost(default_cost);
        obs.param2double(&cost);
        costs.push_back(cost);
    }

    for (int i = 0; i < g->nodes().size() and not costs.empty(); ++i)
    if (g->node(i).type() == pg::NODE_OBSERVABLE)
        indices.push_back(i);

    assert(indices.size() == costs.size());

    for (int i = 0; i < indices.size(); ++i)
        (*out)[indices.at(i)] = costs.at(i);
}


void weighted_converter_t::cost_provider_t::get_hypothesis_costs(
    const pg::proof_graph_t *g,
    const weight_provider_t &weight_prv, const cost_operator_t &cost_opr,
    node2cost_map_t *node2cost)
{
    /** BECAUSE depth > 0,
     *  WE HAVE NO NEED TO PAY ATTENTION TO UNIFICATION-ASSUMPTIONS. */

    for (int depth = 1;; ++depth)
    {
        const hash_set<pg::node_idx_t> *nodes = g->search_nodes_with_depth(depth);
        if (nodes == NULL) break;

        hash_set<pg::hypernode_idx_t> hns;
        for (auto n : (*nodes))
            hns.insert(g->node(n).master_hypernode());

        for (auto hn : hns)
        {
            pg::edge_idx_t parent = g->find_parental_edge(hn);
            if (parent < 0) continue;

            const pg::edge_t edge = g->edge(parent);
            assert(edge.is_chain_edge());

            // COMPUTE SUM OF COST IN TAIL
            double cost_from(0.0);
            for (auto n : g->hypernode(edge.tail()))
            {
                auto find = node2cost->find(n);
                if (find != node2cost->end())
                    cost_from += find->second;
            }

            const std::vector<pg::node_idx_t> &hn_to = g->hypernode(edge.head());
            std::vector<double> weights = weight_prv(g, parent);
            assert(hn_to.size() == weights.size());

            // ASSIGN COSTS TO HEAD NODES
            for (int i = 0; i < hn_to.size(); ++i)
            {
                double cost = cost_opr(cost_from, weights[i]);
                (*node2cost)[hn_to[i]] = cost;
            }
        }
    }
}


std::vector<double>
weighted_converter_t::cost_provider_t::get_axiom_weights(
const pg::proof_graph_t *g, pg::edge_idx_t idx, double default_weight)
{
    const pg::edge_t &edge = g->edge(idx);
    size_t size = g->hypernode(edge.head()).size();
    lf::axiom_t axiom(kb::kb()->get_axiom(edge.axiom_id()));
    lf::logical_function_t branch =
        axiom.func.branch(edge.type() == pg::EDGE_HYPOTHESIZE ? 0 : 1);

    bool do_use_default(true);
    std::vector<double> weights(size, 0.0);

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

    if (do_use_default)
        weights.assign(size, default_weight / (double)size);

    return weights;
}


/* -------- Methods of xml_decorator_t -------- */


weighted_converter_t::xml_decorator_t::xml_decorator_t(
    const hash_map<pg::node_idx_t, ilp::variable_idx_t> &node2costvar)
    : m_node2costvar(node2costvar)
{}


void weighted_converter_t::xml_decorator_t::get_literal_attributes(
    const ilp::ilp_solution_t *sol, pg::node_idx_t idx,
    hash_map<std::string, std::string> *out) const
{
    auto find = m_node2costvar.find(idx);
    if (find != m_node2costvar.end())
    {
        ilp::variable_idx_t costvar = find->second;
        double cost(sol->problem()->variable(costvar).objective_coefficient());
        (*out)["cost"] = util::format("%lf", cost);
        (*out)["paid-cost"] = sol->variable_is_active(costvar) ? "yes" : "no";
    }
}


/* -------- Methods of basic_cost_provider_t -------- */


weighted_converter_t::basic_cost_provider_t::basic_cost_provider_t(
    const cost_operator_t &opr, double def_cost, double def_weight, const std::string &name)
: m_cost_operator(opr),
m_default_observation_cost(def_cost), m_default_axiom_weight(def_weight), m_name(name)
{}


hash_map<pg::node_idx_t, double>
weighted_converter_t::basic_cost_provider_t::operator()(const pg::proof_graph_t *g) const
{
    hash_map<pg::node_idx_t, double> node2cost;
    auto _get_weights =
        std::bind(get_axiom_weights, std::placeholders::_1, std::placeholders::_2, m_default_axiom_weight);

    get_observation_costs(g, m_default_observation_cost, &node2cost);
    get_hypothesis_costs(g, _get_weights, m_cost_operator, &node2cost);

    return node2cost;
}



/* -------- Methods of parameterized_cost_provider_t -------- */


weighted_converter_t::parameterized_cost_provider_t::parameterized_cost_provider_t()
{}


weighted_converter_t::parameterized_cost_provider_t::
parameterized_cost_provider_t(const parameterized_cost_provider_t &p)
: m_weights(p.m_weights)
{}


hash_map<pg::node_idx_t, double> weighted_converter_t::
parameterized_cost_provider_t::operator()(const pg::proof_graph_t *g) const
{
    auto _get_weights =
        std::bind(get_weights, std::placeholders::_1, std::placeholders::_2, &m_weights);

    hash_map<pg::node_idx_t, double> node2cost;
    get_observation_costs(g, 10.0, &node2cost);
    get_hypothesis_costs(g, _get_weights, std::multiplies<double>(), &node2cost);

    return node2cost;
}


void weighted_converter_t::parameterized_cost_provider_t::train(
    const ilp::ilp_solution_t &sys, const ilp::ilp_solution_t &gold,
    util::xml_element_t *out)
{
    // TODO
}


void weighted_converter_t::parameterized_cost_provider_t::load(const std::string &filename)
{
    m_weights.clear();

    std::ifstream fin(filename);
    char line[256];

    if (not fin)
    {
        util::print_warning_fmt(
            "cannot open feature-weight file: \"%s\"", filename.c_str());
        return;
    }

    while (fin.good() and not fin.eof())
    {
        fin.getline(line, 256);
        auto splitted = util::split(line, "\t");

        if (splitted.size() == 2)
        {
            double w;
            _sscanf(splitted.back().c_str(), "%lf", &w);
            m_weights[splitted.front()] = w;
        }
    }
}


void weighted_converter_t::parameterized_cost_provider_t::load(const feature_weights_t &weights)
{
    m_weights = weights;
}


void weighted_converter_t::parameterized_cost_provider_t::write(const std::string &filename) const
{
    std::ofstream fout(filename);

    if (not fout)
    {
        util::print_warning_fmt(
            "cannot open feature-weight file: \"%s\"", filename.c_str());
        return;
    }

    for (auto p : m_weights)
        fout << p.first << '\t' << p.second << std::endl;
}


double get_random_weight()
{
    static std::mt19937 mt(std::random_device().operator()());
    return std::uniform_real_distribution<double>(-1.0, 1.0).operator()(mt);
}


std::vector<double> weighted_converter_t::parameterized_cost_provider_t
::get_weights(const pg::proof_graph_t *g, pg::edge_idx_t idx, feature_weights_t *weights)
{

    const pg::edge_t &edge = g->edge(idx);
    size_t size = g->hypernode(edge.head()).size();

    lf::axiom_t axiom(kb::kb()->get_axiom(edge.axiom_id()));
    lf::logical_function_t branch =
        axiom.func.branch(edge.type() == pg::EDGE_HYPOTHESIZE ? 0 : 1);

    hash_set<std::string> features;
    // TODO: ‘f«‚ð—ñ‹“‚·‚é

    double sum(0.0);
    for (auto f : features)
    {
        auto found = weights->find(f);

        if (found == weights->end())
        {
            double init = get_random_weight();
            (*weights)[f] = init;
            sum += init;
        }
        else
            sum += found->second;
    }

    double weight = (2.0 + std::tanh(sum)) / (double)size;
    return std::vector<double>(size, weight);
}



}

}

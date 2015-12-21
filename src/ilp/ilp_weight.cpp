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
            std::multiplies<double>(), def_cost, def_weight, "basic-multiply");

        if (key == "linear")
            return new basic_cost_provider_t(
            std::plus<double>(), def_cost, def_weight, "basic-addition");

        if (key == "parameterized")
            return
            new parameterized_cost_provider_t(
            ph->param("model"), ph->param("retrain"),
            opt::generate_optimizer(ph->param("optimizer")),
            opt::generate_loss_function(ph->param("loss"), false),
            opt::generate_activation_function(ph->param("activation")));

        if (key == "parameterized-linear")
            return
            new parameterized_linear_cost_provider_t(
            ph->param("model"), ph->param("retrain"),
            opt::generate_optimizer(ph->param("optimizer")),
            opt::generate_loss_function(ph->param("loss"), false),
            opt::generate_activation_function(ph->param("activation")));

        throw phillip_exception_t(
            "The arguments for cost-provider are invalid: " + key);
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
    ilp_problem_t *prob = new ilp_problem_t(graph);

    convert_proof_graph(prob);
    if (prob->has_timed_out()) return prob;

#define _check_timeout if(do_time_out(begin)) { prob->timeout(true); return prob; }
    
    for (auto p : (*m_cost_provider)(graph))
    {
        prob->add_variable_for_hypothesis_cost(p.first, p.second);
        if (do_time_out(begin)) break;
    }

    for (auto p : prob->hypo_cost_map())
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

                    double cost1 = prob->get_hypothesis_cost_of(from[0]);
                    double cost2 = prob->get_hypothesis_cost_of(from[1]);
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
        double cost1 = prob->get_hypothesis_cost_of(from[0]);
        double cost2 = prob->get_hypothesis_cost_of(from[1]);
        pg::node_idx_t
            explained((cost1 > cost2) ? from[0] : from[1]),
            explains((cost1 > cost2) ? from[1] : from[0]);

        prob->add_constraints_to_forbid_chaining_from_explained_node(i, explained);
        prob->add_constraints_to_forbid_looping_unification(i, explained);

        if (do_time_out(begin)) break;
    }

#undef _check_timeout
    
    return prob;
}


bool weighted_converter_t::is_available(std::list<std::string> *message) const
{
    return m_cost_provider->is_available(message);
}


void weighted_converter_t::write(std::ostream *os) const
{
    (*os) << "<converter name=\"weighted\">" << std::endl;

    m_cost_provider->write(os);

    (*os) << "</converter>" << std::endl;
}


opt::training_result_t* weighted_converter_t::train(
    opt::epoch_t epoch,
    const ilp::ilp_solution_t &sys, const ilp::ilp_solution_t &gold)
{
    return m_cost_provider->train(epoch, sys, gold);
}


bool weighted_converter_t::is_trainable(std::list<std::string> *messages) const
{
    return m_cost_provider->is_trainable(messages);
}


ilp_converter_t* weighted_converter_t::
generator_t::operator()(const phillip_main_t *ph) const
{
    return new weighted_converter_t(
        ph, weighted_converter_t::generate_cost_provider(ph));
}


/* -------- Methods of ilp_problem_t -------- */

weighted_converter_t::ilp_problem_t::ilp_problem_t(const pg::proof_graph_t *graph)
: ilp::ilp_problem_t(graph, new ilp::basic_solution_interpreter_t(), false)
{
    add_xml_decorator(new xml_decorator_t(this));
}


ilp::variable_idx_t weighted_converter_t::ilp_problem_t::add_variable_for_hypothesis_cost(pg::node_idx_t idx, double cost)
{
    ilp::variable_idx_t v(find_variable_with_node(idx));
    if (v >= 0)
    {
        std::string name = util::format("cost(n:%d)", idx);
        ilp::variable_idx_t costvar =
            add_variable(ilp::variable_t(name, cost));
        m_hypo_cost_map[idx] = costvar;

        return costvar;
    }
    else
        return -1;
}


double weighted_converter_t::ilp_problem_t::get_hypothesis_cost_of(pg::node_idx_t idx) const
{
    auto find = m_hypo_cost_map.find(idx);
    return (find != m_hypo_cost_map.end()) ? variable(find->second).objective_coefficient() : 0.0;
}


weighted_converter_t::ilp_problem_t::xml_decorator_t::xml_decorator_t(const ilp_problem_t *master)
: m_master(master)
{}


void weighted_converter_t::ilp_problem_t::xml_decorator_t::get_literal_attributes(
    const ilp::ilp_solution_t *sol, pg::node_idx_t idx,
    hash_map<std::string, std::string> *out) const
{
    const hash_map<pg::node_idx_t, ilp::variable_idx_t> &map = m_master->hypo_cost_map();
    auto find = map.find(idx);

    if (find != map.end())
    {
        ilp::variable_idx_t costvar = find->second;
        double cost(sol->problem()->variable(costvar).objective_coefficient());
        (*out)["cost"] = util::format("%lf", cost);
        (*out)["paid-cost"] = sol->variable_is_active(costvar) ? "yes" : "no";
    }
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


bool weighted_converter_t::basic_cost_provider_t::is_available(std::list<std::string> *message) const
{
    return true;
}


void weighted_converter_t::basic_cost_provider_t::write(std::ostream *os) const
{
    (*os)
        << "<cost-provider name=\"" << m_name
        << "\" default-observation-cost=\"" << m_default_observation_cost
        << "\" default-axiom-weight=\"" << m_default_axiom_weight
        << "\"></cost-provider>" << std::endl;
}



/* -------- Methods of virtual_parameterized_cost_provider_t -------- */


weighted_converter_t::virtual_parameterized_cost_provider_t::virtual_parameterized_cost_provider_t(
    const file_path_t &model, const file_path_t &model_for_retrain,
    opt::optimization_method_t *optimizer,
    opt::loss_function_t *error,
    opt::activation_function_t *hypo_cost_provider)
    : m_model_path(model), m_model_path_for_retrain(model_for_retrain),
    m_optimizer(optimizer), m_loss_function(error),
    m_hypothesis_cost_provider(hypo_cost_provider)
{}


void weighted_converter_t::virtual_parameterized_cost_provider_t::prepare_train()
{
    m_weights.reset(new opt::feature_weights_t());
    if (not m_model_path_for_retrain.empty())
        m_weights->load(m_model_path_for_retrain);
}


void weighted_converter_t::virtual_parameterized_cost_provider_t::postprocess_train()
{
    m_weights->write(m_model_path);
}


opt::training_result_t* weighted_converter_t::virtual_parameterized_cost_provider_t::train(
    opt::epoch_t epoch,
    const ilp::ilp_solution_t &sys, const ilp::ilp_solution_t &gold)
{
    double loss = m_loss_function->get(
        gold.value_of_objective_function(), sys.value_of_objective_function());
    opt::training_result_t *out = new opt::training_result_t(epoch, loss);

    hash_map<opt::feature_t, opt::gradient_t> grads = get_gradients(epoch, sys, gold);
    for (auto p : grads)
    {
        const opt::feature_t f = p.first;
        const opt::gradient_t g = p.second;
        opt::weight_t *w = &(*m_weights)[f];
        opt::weight_t w_old = (*w);

        m_optimizer->update(w, g, epoch);
        out->add(f, g, w_old, *w);
    }

    return out;
}


bool weighted_converter_t::virtual_parameterized_cost_provider_t::
is_available(std::list<std::string> *message) const
{
    bool out(true);

    if (m_model_path.empty())
    {
        message->push_back(
            "virtual_parameterized_cost_provider_t: The model path is not specified.");
        out = false;
    }

    if (not m_hypothesis_cost_provider)
    {
        message->push_back(
            "virtual_parameterized_cost_provider_t: The cost provider for hypothesis cost is lacked.");
        out = false;
    }

    return out;
}


bool weighted_converter_t::virtual_parameterized_cost_provider_t::
is_trainable(std::list<std::string> *messages) const
{
    bool out(true);

    if (not m_optimizer)
    {
        messages->push_back(
            "virtual_parameterized_cost_provider_t: The optimizer is lacked.");
        out = false;
    }

    if (not m_loss_function)
    {
        messages->push_back(
            "virtual_parameterized_cost_provider_t: The loss function is lacked.");
        out = false;
    }

    return out;
}


void weighted_converter_t::virtual_parameterized_cost_provider_t
::get_weights(const pg::proof_graph_t *g, pg::edge_idx_t idx, std::vector<opt::weight_t> *out) const
{
    const pg::edge_t &edge = g->edge(idx);
    size_t size = g->hypernode(edge.head()).size();

    hash_set<opt::feature_t> features;
    get_features(g, idx, &features);

    double w = m_hypothesis_cost_provider->operate(features, (*m_weights));
    out->assign(size, (w / (double)size));
}


void weighted_converter_t::virtual_parameterized_cost_provider_t::get_features(
    const pg::proof_graph_t *graph, pg::edge_idx_t idx, hash_set<opt::feature_t> *out) const
{
    const pg::edge_t &edge = graph->edge(idx);
    lf::axiom_t axiom(kb::kb()->get_axiom(edge.axiom_id()));

    // CHECK MEMORY
    {
        auto found = m_ax2ft.find(edge.axiom_id());
        if (found != m_ax2ft.end())
        {
            out->insert(found->second.begin(), found->second.end());
            return;
        }
    }

    hash_set<opt::feature_t> &feats = m_ax2ft[edge.axiom_id()];

    feats.insert(util::format("id/%d", edge.axiom_id()));

    for (auto l1 : axiom.func.get_lhs())
    for (auto l2 : axiom.func.get_rhs())
        feats.insert("p/" + l1->get_arity() + "/" + l2->get_arity());

    axiom.func.process_parameter([&feats](const std::string &s)
    {
        if (s.size() >= 3)
        if (s.at(0) == 'f')
        if (s.at(1) == '/')
            feats.insert(s.substr(2));
        return false;
    });

    out->insert(feats.begin(), feats.end());
}



/* -------- Methods of parameterized_cost_provider_t -------- */


weighted_converter_t::parameterized_cost_provider_t::parameterized_cost_provider_t(
    const file_path_t &model, const file_path_t &model_for_retrain,
    opt::optimization_method_t *optimizer, opt::loss_function_t *error,
    opt::activation_function_t *hypo_cost_provider)
    : virtual_parameterized_cost_provider_t(model, model_for_retrain, optimizer, error, hypo_cost_provider)
{}


hash_map<pg::node_idx_t, double> weighted_converter_t::
parameterized_cost_provider_t::operator()(const pg::proof_graph_t *g) const
{
    auto _get_weights = [this](const pg::proof_graph_t *g, pg::edge_idx_t i)
    {
        std::vector<double> out;
        this->get_weights(g, i, &out);
        return out;
    };

    hash_map<pg::node_idx_t, double> node2cost;
    get_observation_costs(g, 10.0, &node2cost);
    get_hypothesis_costs(g, _get_weights, std::multiplies<double>(), &node2cost);

    return node2cost;
}


void weighted_converter_t::parameterized_cost_provider_t::write(std::ostream *os) const
{
    (*os) << "<cost-provider name=\"parameterized\">" << std::endl;
    m_optimizer->write(os);
    m_loss_function->write(os);
    m_hypothesis_cost_provider->write("hypothesis-weight", os);
    (*os) << "</cost-provider>" << std::endl;
}



hash_map<opt::feature_t, opt::gradient_t>
weighted_converter_t::parameterized_cost_provider_t::get_gradients(
opt::epoch_t epoch, const ilp::ilp_solution_t &sys, const ilp::ilp_solution_t &gold) const
{
    const ilp_problem_t *prob_sys = static_cast<const ilp_problem_t*>(sys.problem());
    const ilp_problem_t *prob_gold = static_cast<const ilp_problem_t*>(gold.problem());
    const pg::proof_graph_t *graph = sys.proof_graph();
    double obj_sys = sys.value_of_objective_function();
    double obj_gold = gold.value_of_objective_function();
    hash_map<opt::feature_t, opt::gradient_t> out;

    std::function<void(
        const pg::proof_graph_t*, pg::node_idx_t, opt::gradient_t,
        hash_map<pg::edge_idx_t, opt::gradient_t>*)> backpropagate;

    backpropagate = [&](
    const pg::proof_graph_t *graph, pg::node_idx_t target, opt::gradient_t grad,
    hash_map<pg::edge_idx_t, opt::gradient_t> *out)
    {
        pg::hypernode_idx_t master = graph->node(target).master_hypernode();
        if (master < 0) return;

        hash_set<pg::edge_idx_t> edges;
        auto hn = graph->hypernode(master);
        graph->enumerate_parental_edges(master, &edges);

        for (auto e : edges)
        if (graph->edge(e).is_chain_edge())
        {
            if (out->count(e) > 0) (*out)[e] += grad;
            else                   (*out)[e] = grad;

            std::vector<opt::weight_t> w;
            get_weights(graph, e, &w);
            assert(w.size() == hn.size());

            for (int i = 0; i < hn.size(); ++i)
            if (hn.at(i) == target)
            {
                opt::gradient_t g_p = grad * w.at(i);
                auto parents = graph->hypernode(graph->edge(e).tail());
                for (auto n : parents)
                if (graph->node(n).type() == pg::NODE_HYPOTHESIS)
                    backpropagate(graph, n, g_p, out);
            }
        }
    };

    for (int i = 0; i < 2; ++i)
    {
        bool is_gold(i == 0);
        const ilp::ilp_solution_t &sol = (is_gold ? gold : sys);
        const ilp_problem_t *prob = (is_gold ? prob_gold : prob_sys);
        opt::gradient_t grad = is_gold ?
            m_loss_function->gradient_true(obj_gold, obj_sys) :
            m_loss_function->gradient_false(obj_gold, obj_sys);
        hash_map<pg::edge_idx_t, opt::gradient_t> e2g;

        for (auto p : prob->hypo_cost_map())
        if (sol.variable_is_active(p.second))
            backpropagate(graph, p.first, grad, &e2g);

        for (auto p : e2g)
        {
            hash_set<opt::feature_t> feats;
            get_features(graph, p.first, &feats);
            m_hypothesis_cost_provider->backpropagate(feats, (*m_weights), p.second, &out);
        }
    }

    return out;
}



/* -------- Methods of parameterized_linear_cost_provider_t -------- */


weighted_converter_t::parameterized_linear_cost_provider_t::parameterized_linear_cost_provider_t(
    const file_path_t &model, const file_path_t &model_for_retrain,
    opt::optimization_method_t *optimizer, opt::loss_function_t *error,
    opt::activation_function_t *hypo_cost_provider)
    : virtual_parameterized_cost_provider_t(model, model_for_retrain, optimizer, error, hypo_cost_provider)
{}


hash_map<pg::node_idx_t, double> weighted_converter_t::
parameterized_linear_cost_provider_t::operator()(const pg::proof_graph_t *g) const
{
    auto _get_weights = [this](const pg::proof_graph_t *g, pg::edge_idx_t i)
    {
        std::vector<double> out;
        this->get_weights(g, i, &out);
        return out;
    };

    hash_map<pg::node_idx_t, double> node2cost;
    get_observation_costs(g, 10.0, &node2cost);
    get_hypothesis_costs(g, _get_weights, std::plus<double>(), &node2cost);

    return node2cost;
}


void weighted_converter_t::parameterized_linear_cost_provider_t::write(std::ostream *os) const
{
    (*os) << "<cost-provider name=\"parameterized-linear\">" << std::endl;
    m_optimizer->write(os);
    m_loss_function->write(os);
    m_hypothesis_cost_provider->write("hypothesis-weight", os);
    (*os) << "</cost-provider>" << std::endl;
}



hash_map<opt::feature_t, opt::gradient_t>
weighted_converter_t::parameterized_linear_cost_provider_t::get_gradients(
    opt::epoch_t epoch, const ilp::ilp_solution_t &sys, const ilp::ilp_solution_t &gold) const
{
    const ilp_problem_t *prob_sys = static_cast<const ilp_problem_t*>(sys.problem());
    const ilp_problem_t *prob_gold = static_cast<const ilp_problem_t*>(gold.problem());
    const pg::proof_graph_t *graph = sys.proof_graph();
    double obj_sys = sys.value_of_objective_function();
    double obj_gold = gold.value_of_objective_function();
    hash_map<opt::feature_t, opt::gradient_t> out;

    for (int i = 0; i < 2; ++i)
    {
        bool is_gold(i == 0);
        const ilp::ilp_solution_t &sol = (is_gold ? gold : sys);
        const ilp_problem_t *prob = (is_gold ? prob_gold : prob_sys);
        hash_set<pg::edge_idx_t> chains;

        for (auto p : prob->hypo_cost_map())
        if (sol.variable_is_active(p.second))
        {
            hash_set<pg::edge_idx_t> c = graph->enumerate_dependent_edges(p.first);
            chains.insert(c.begin(), c.end());
        }

        for (auto idx_e : chains)
        {
            const pg::edge_t edge = graph->edge(idx_e);
            hash_set<opt::feature_t> feats;
            opt::gradient_t g = is_gold ?
                m_loss_function->gradient_true(obj_gold, obj_sys) :
                m_loss_function->gradient_false(obj_gold, obj_sys);

            get_features(graph, idx_e, &feats);
            m_hypothesis_cost_provider->backpropagate(feats, (*m_weights), g, &out);
        }
    }

    return out;
}


}

}

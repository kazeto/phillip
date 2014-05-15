#pragma once

#include "../phillip.h"

namespace phil
{

/** A namespace about factories of linear-programming-problems. */
namespace ilp
{


class null_converter_t : public ilp_converter_t
{
public:
    null_converter_t() {}
    virtual ilp::ilp_problem_t* execute() const;
    virtual bool is_available(std::list<std::string>*) const;
    virtual std::string repr() const;
};


/** A class of ilp-converter for a weight-based evaluation function. */
class weighted_converter_t : public ilp_converter_t
{
public:
    class weight_provider_t {
    public:
        virtual ~weight_provider_t() {}
        virtual std::vector<double> operator()(
            const pg::proof_graph_t*, pg::edge_idx_t) const = 0;
    };

    class basic_weight_provider_t : public weight_provider_t {
    public:
        basic_weight_provider_t(double default_weight) : m_default_weight(default_weight) {}
        virtual std::vector<double> operator()(
            const pg::proof_graph_t*, pg::edge_idx_t) const;
    private:
        double m_default_weight;
    };

    class my_xml_decorator_t : public ilp::solution_xml_decorator_t {
    public:
        my_xml_decorator_t(
            const hash_map<pg::node_idx_t, ilp::variable_idx_t> &node2costvar);
        virtual void get_literal_attributes(
            const ilp_solution_t *sol, pg::node_idx_t idx,
            hash_map<std::string, std::string> *out) const;
        virtual void get_edge_attributes(
            const ilp_solution_t *sol, pg::edge_idx_t idx,
            hash_map<std::string, std::string> *out) const {}
    private:
        hash_map<pg::node_idx_t, ilp::variable_idx_t> m_node2costvar;
    };

    class my_enumeration_stopper_t : public ilp_converter_t::enumeration_stopper_t {
        virtual bool operator()(const pg::proof_graph_t*) const;
    };

    static weight_provider_t* parse_string_to_weight_provider(const std::string &str);

    weighted_converter_t(
        double default_obs_cost = 10.0,
        weight_provider_t *ptr = NULL);
    ~weighted_converter_t();

    virtual ilp::ilp_problem_t* execute() const;
    virtual bool is_available(std::list<std::string>*) const;
    virtual std::string repr() const;

protected:
    void add_variable_for_cost(
        pg::node_idx_t idx, double cost, ilp::ilp_problem_t *prob,
        hash_map<pg::node_idx_t, ilp::variable_idx_t> *node2costvar) const;
    void add_variables_for_observation_cost(
        const pg::proof_graph_t *graph,
        const lf::input_t &input, ilp::ilp_problem_t *prob,
        hash_map<pg::node_idx_t, ilp::variable_idx_t> *node2costvar) const;
    void add_variables_for_hypothesis_cost(
        const pg::proof_graph_t *graph, ilp::ilp_problem_t *prob,
        hash_map<pg::node_idx_t, ilp::variable_idx_t> *node2costvar) const;
    void add_constraints_for_cost(
        const pg::proof_graph_t *graph, ilp::ilp_problem_t *prob,
        const hash_map<pg::node_idx_t, ilp::variable_idx_t> &node2costvar) const;
    double get_cost(
        pg::node_idx_t idx, const ilp::ilp_problem_t *prob,
        const hash_map<pg::node_idx_t, ilp::variable_idx_t> &node2costvar) const;

    double m_default_observation_cost;
    weight_provider_t *m_weight_provider;
};


/** A class of ilp-converter for a cost-based evaluation function. */
class costed_converter_t : public ilp_converter_t
{
public:
    class cost_provider_t {
    public:
        virtual ~cost_provider_t() {}
        virtual double edge_cost(
            const pg::proof_graph_t*, pg::edge_idx_t) const = 0;
        virtual double node_cost(
            const pg::proof_graph_t*, pg::node_idx_t) const = 0;
    };

    class basic_cost_provider_t : public cost_provider_t {
    public:
        basic_cost_provider_t(
            double default_cost, double literal_unify_cost, double term_unify_cost);
        virtual double edge_cost(const pg::proof_graph_t*, pg::edge_idx_t) const;
        virtual double node_cost(const pg::proof_graph_t*, pg::node_idx_t) const;
    private:
        double m_default_axiom_cost, m_literal_unifying_cost, m_term_unifying_cost;
    };

    class my_enumeration_stopper_t : public ilp_converter_t::enumeration_stopper_t {
        virtual bool operator()(const pg::proof_graph_t*) const;
    };

    static cost_provider_t* parse_string_to_cost_provider(const std::string&);

    costed_converter_t(cost_provider_t *ptr = NULL);
    ~costed_converter_t();

    virtual ilp::ilp_problem_t* execute() const;
    virtual bool is_available(std::list<std::string>*) const;
    virtual std::string repr() const;

protected:
    cost_provider_t *m_cost_provider;
};

}

}


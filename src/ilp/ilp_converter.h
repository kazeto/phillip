#pragma once

#include <memory>
#include <functional>
#include "../phillip.h"

namespace phil
{

/** A namespace about factories of linear-programming-problems. */
namespace cnv
{


class null_converter_t : public ilp_converter_t
{
public:
    struct generator_t : public component_generator_t<ilp_converter_t>
    {
        virtual ilp_converter_t* operator()(const phillip_main_t*) const override;
    };

    null_converter_t(const phillip_main_t *ptr) : ilp_converter_t(ptr) {}

    virtual ilp_converter_t* duplicate(const phillip_main_t *ptr) const override;
    virtual ilp::ilp_problem_t* execute() const override;
    virtual bool is_available(std::list<std::string>*) const override;
    virtual std::string repr() const override;
    virtual bool do_keep_validity_on_timeout() const override { return true; }
};


/** A class of ilp-converter for a weight-based evaluation function. */
class weighted_converter_t : public ilp_converter_t
{
public:
    typedef hash_map<pg::node_idx_t, double> node2cost_map_t;

    struct generator_t : public component_generator_t<ilp_converter_t>
    {
        virtual ilp_converter_t* operator()(const phillip_main_t*) const override;
    };

    /** A virtual class of function object which computes hypothesizing cost on each node. */
    class cost_provider_t
    {
    public:
        virtual hash_map<pg::node_idx_t, double> operator()(const pg::proof_graph_t *g) const = 0;
        virtual cost_provider_t* duplicate() const = 0;

        virtual void train(
            const ilp::ilp_solution_t &sys, const ilp::ilp_solution_t &gold,
            util::xml_element_t *out) = 0;

        virtual std::string repr() const = 0;

    protected:
        typedef std::function<std::vector<double>(const pg::proof_graph_t*, pg::edge_idx_t)> weight_provider_t;
        typedef std::function<double(double, double)> cost_operator_t;

        static void get_observation_costs(
            const pg::proof_graph_t *g, double default_cost, node2cost_map_t *out);
        static void get_hypothesis_costs(
            const pg::proof_graph_t *g, const weight_provider_t &weight_prv,
            const cost_operator_t &cost_opr, node2cost_map_t *out);
        static std::vector<double> get_axiom_weights(const pg::proof_graph_t*, pg::edge_idx_t, double);
    };

    /** A super class of cost_provider_t, which is basic. */
    class basic_cost_provider_t : public cost_provider_t
    {
    public:
        basic_cost_provider_t(
            const cost_operator_t &opr, double def_obs_cost, double def_weight,
            const std::string &name);

        virtual hash_map<pg::node_idx_t, double> operator()(const pg::proof_graph_t *g) const override;
        virtual cost_provider_t*duplicate() const;
        virtual void train(
            const ilp::ilp_solution_t &sys, const ilp::ilp_solution_t &gold,
            util::xml_element_t *out) {}
        virtual std::string repr() const { return "basic"; }

    protected:
        std::string m_name;
        cost_operator_t m_cost_operator;
        double m_default_observation_cost;
        double m_default_axiom_weight;
    };

    /** A cost provider which compute axiom's weight from its feature weights.
     *  These feature weights can be trainable. */
    class parameterized_cost_provider_t : public cost_provider_t
    {
    public:
        typedef hash_map<std::string, double> feature_weights_t;
        parameterized_cost_provider_t();
        parameterized_cost_provider_t(const std::string &filename);
        parameterized_cost_provider_t(const feature_weights_t &weights);

        virtual hash_map<pg::node_idx_t, double> operator()(const pg::proof_graph_t *g) const override;
        virtual cost_provider_t*duplicate() const;
        virtual void train(
            const ilp::ilp_solution_t &sys, const ilp::ilp_solution_t &gold,
            util::xml_element_t *out);
        virtual std::string repr() const { return "parameterized"; }

        void write(const std::string &filename) const; /// Outputs the feature weights.

    protected:
        static std::vector<double> get_weights(const pg::proof_graph_t*, pg::edge_idx_t, feature_weights_t*);

        mutable feature_weights_t m_weights; /// Feature weights learned.
    };

    class xml_decorator_t : public ilp::solution_xml_decorator_t
    {
    public:
        xml_decorator_t(const hash_map<pg::node_idx_t, ilp::variable_idx_t> &node2costvar);
        virtual void get_literal_attributes(
            const ilp::ilp_solution_t *sol, pg::node_idx_t idx,
            hash_map<std::string, std::string> *out) const;

    private:
        hash_map<pg::node_idx_t, ilp::variable_idx_t> m_node2costvar;
    };

    static cost_provider_t* generate_cost_provider(const phillip_main_t *ph);

    weighted_converter_t(const phillip_main_t *main, cost_provider_t *ptr);

    virtual ilp_converter_t* duplicate(const phillip_main_t *ptr) const override;
    virtual ilp::ilp_problem_t* execute() const override;
    virtual bool is_available(std::list<std::string>*) const override;
    virtual std::string repr() const override;
    virtual bool do_keep_validity_on_timeout() const override { return false; }

protected:
    std::unique_ptr<cost_provider_t> m_cost_provider;
};


/** A class of ilp-converter for a cost-based evaluation function. */
class costed_converter_t : public ilp_converter_t
{
public:
    struct generator_t : public component_generator_t<ilp_converter_t>
    {
        virtual ilp_converter_t* operator()(const phillip_main_t*) const override;
    };

    class cost_provider_t {
    public:
        virtual ~cost_provider_t() {}
        virtual cost_provider_t* duplicate() const = 0;
        virtual double edge_cost(const pg::proof_graph_t*, pg::edge_idx_t) const = 0;
        virtual double node_cost(const pg::proof_graph_t*, pg::node_idx_t) const = 0;
    };

    class basic_cost_provider_t : public cost_provider_t {
    public:
        basic_cost_provider_t(
            double default_cost, double literal_unify_cost, double term_unify_cost);
        virtual cost_provider_t* duplicate() const;
        virtual double edge_cost(const pg::proof_graph_t*, pg::edge_idx_t) const;
        virtual double node_cost(const pg::proof_graph_t*, pg::node_idx_t) const;
    private:
        double m_default_axiom_cost, m_literal_unifying_cost, m_term_unifying_cost;
    };

    static cost_provider_t* parse_string_to_cost_provider(const std::string&);

    costed_converter_t(const phillip_main_t *main, cost_provider_t *ptr = NULL);
    ~costed_converter_t();

    virtual ilp_converter_t* duplicate(const phillip_main_t *ptr) const override;
    virtual ilp::ilp_problem_t* execute() const override;
    virtual bool is_available(std::list<std::string>*) const override;
    virtual std::string repr() const override;
    virtual bool do_keep_validity_on_timeout() const override { return false; }

protected:
    cost_provider_t *m_cost_provider;
};


}

}


#pragma once

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
        virtual ilp_converter_t* operator()(phillip_main_t*) const override;
    };

    null_converter_t(phillip_main_t *ptr) : ilp_converter_t(ptr) {}
    virtual ilp_converter_t* duplicate(phillip_main_t *ptr) const;
    virtual ilp::ilp_problem_t* execute() const;
    virtual bool is_available(std::list<std::string>*) const;
    virtual std::string repr() const;
};


/** A class of ilp-converter for a weight-based evaluation function. */
class weighted_converter_t : public ilp_converter_t
{
public:
    struct generator_t : public component_generator_t<ilp_converter_t>
    {
        virtual ilp_converter_t* operator()(phillip_main_t*) const override;
    };

    class weight_provider_t {
    public:
        virtual ~weight_provider_t() {}
        virtual std::vector<double> operator()(
            const pg::proof_graph_t*, pg::edge_idx_t) const = 0;
        virtual weight_provider_t* duplicate() const = 0;
    };

    class basic_weight_provider_t : public weight_provider_t {
    public:
        basic_weight_provider_t(double default_weight) : m_default_weight(default_weight) {}
        virtual std::vector<double> operator()(
            const pg::proof_graph_t*, pg::edge_idx_t) const;
        virtual weight_provider_t* duplicate() const;
    private:
        double m_default_weight;
    };

    class my_xml_decorator_t : public ilp::solution_xml_decorator_t {
    public:
        my_xml_decorator_t(
            const hash_map<pg::node_idx_t, ilp::variable_idx_t> &node2costvar);
        virtual void get_literal_attributes(
            const ilp::ilp_solution_t *sol, pg::node_idx_t idx,
            hash_map<std::string, std::string> *out) const;
    private:
        hash_map<pg::node_idx_t, ilp::variable_idx_t> m_node2costvar;
    };

    class my_enumeration_stopper_t : public ilp_converter_t::enumeration_stopper_t {
    public:
        my_enumeration_stopper_t(const weighted_converter_t *ptr) : m_converter(ptr) {}
        virtual bool operator()(const pg::proof_graph_t*);
    private:
        const weighted_converter_t *m_converter;
        hash_set<pg::edge_idx_t> m_considered_edges;
    };

    static weight_provider_t* parse_string_to_weight_provider(const std::string &str);

    weighted_converter_t(
        phillip_main_t *main,
        double default_obs_cost = 10.0, weight_provider_t *ptr = NULL,
        bool is_logarithmic = false);
    ~weighted_converter_t();

    virtual ilp_converter_t* duplicate(phillip_main_t *ptr) const;
    virtual ilp::ilp_problem_t* execute() const;
    virtual bool is_available(std::list<std::string>*) const;
    virtual std::string repr() const;

    virtual enumeration_stopper_t* enumeration_stopper() const;

    inline std::vector<double> get_weights(
        const pg::proof_graph_t*, pg::edge_idx_t) const;

protected:
    double m_default_observation_cost;
    bool m_is_logarithmic;
    weight_provider_t *m_weight_provider;
};


/** A class of ilp-converter for a cost-based evaluation function. */
class costed_converter_t : public ilp_converter_t
{
public:
    struct generator_t : public component_generator_t<ilp_converter_t>
    {
        virtual ilp_converter_t* operator()(phillip_main_t*) const override;
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

    class my_enumeration_stopper_t : public ilp_converter_t::enumeration_stopper_t {
        virtual bool operator()(const pg::proof_graph_t*) const;
    };

    static cost_provider_t* parse_string_to_cost_provider(const std::string&);

    costed_converter_t(phillip_main_t *main, cost_provider_t *ptr = NULL);
    ~costed_converter_t();

    virtual ilp_converter_t* duplicate(phillip_main_t *ptr) const;
    virtual ilp::ilp_problem_t* execute() const;
    virtual bool is_available(std::list<std::string>*) const;
    virtual std::string repr() const;

protected:
    cost_provider_t *m_cost_provider;
};



/* -------- INLINE METHODS -------- */


std::vector<double> weighted_converter_t::get_weights(
    const pg::proof_graph_t *graph, pg::edge_idx_t i) const
{
    return (*m_weight_provider)(graph, i);
}



}

}


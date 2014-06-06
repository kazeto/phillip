/* -*- coding: utf-8 -*- */

#ifndef INCLUDE_HENRY_LP_PROBLEM_INLINE_H
#define INCLUDE_HENRY_LP_PROBLEM_INLINE_H


namespace phil
{

namespace ilp
{


inline variable_t::variable_t(const std::string &name, double coef)
    : m_name(name), m_objective_coefficient(coef)
{}


inline void variable_t::set_coefficient(double coef)
{
    m_objective_coefficient = coef;
}


inline const std::string& variable_t::name() const
{
    return m_name;
}


inline double variable_t::objective_coefficient() const
{
    return m_objective_coefficient;
}


inline std::string variable_t::to_string() const
{
    return m_name + "=?";
}


inline constraint_t::constraint_t()
    : m_operator(OPR_UNDERSPECIFIED)
{
    m_target[0] = m_target[1] = 0.0;
}


inline constraint_t::constraint_t(
    const std::string &name, constraint_operator_e opr )
    : m_name(name), m_operator(opr)
{
    m_target[0] = m_target[1] = 0.0;
}


inline constraint_t::constraint_t(
    const std::string &name, constraint_operator_e opr, double val )
    : m_name(name), m_operator(opr)
{
    m_target[0] = m_target[1] = val;
}


inline constraint_t::constraint_t(
    const std::string &name, constraint_operator_e opr,
    double val1, double val2 )
    : m_name(name), m_operator(opr)
{
    m_target[0] = val1;
    m_target[1] = val2;
}


inline bool constraint_t::is_empty() const
{ return m_terms.empty(); }


inline void constraint_t::add_term( variable_idx_t var_idx, double coe )
{
    term_t t = { var_idx, coe };
    m_terms.push_back(t);
}

  
inline bool constraint_t::is_satisfied(
    const std::vector<double> &lpsol_optimized_values) const
{
    double val = 0.0;
    for (auto it = m_terms.begin(); it != m_terms.end(); ++it)
    {
        const double &var = lpsol_optimized_values.at(it->var_idx);
        val += var * it->coefficient;
    }
    return _is_satisfied(val);
}


inline bool constraint_t::_is_satisfied( double sol ) const
{
    switch( m_operator )
    {
    case OPR_EQUAL:      return (sol == lower_bound());
    case OPR_LESS_EQ:    return (sol <= upper_bound());
    case OPR_GREATER_EQ: return (sol >= lower_bound());
    case OPR_RANGE:      return (lower_bound() <= sol && sol <= upper_bound());
    default:             return false;
    }
}


inline const std::string& constraint_t::name() const
{
    return m_name;
}


inline constraint_operator_e constraint_t::operator_type() const
{
    return m_operator;
}


inline const std::vector<constraint_t::term_t>& constraint_t::terms() const
{
    return m_terms;
}


inline double constraint_t::bound() const
{
    return m_target[0];
}


inline double constraint_t::lower_bound() const
{
    return m_target[0];
}


inline double constraint_t::upper_bound() const
{
    return m_target[1];
}


inline void constraint_t::set_bound( double lower, double upper )
{
    m_target[0] = lower;
    m_target[1] = upper;
}


inline void constraint_t::set_bound( double target )
{
    m_target[0] = m_target[1] = target;
}


inline std::string constraint_t::to_string(
    const std::vector<variable_t> &vars ) const
{
    std::string exp;
    print(&exp, vars);
    return exp;
}


inline ilp_problem_t::ilp_problem_t(
    const pg::proof_graph_t* lhs, solution_interpreter_t *si,
    bool do_maximize, const std::string &name)
    : m_name(name), m_do_maximize(do_maximize), m_is_timeout(false),
      m_graph(lhs), m_cutoff(INVALID_CUT_OFF), m_solution_interpreter(si)
{}


inline void ilp_problem_t::add_xml_decorator(solution_xml_decorator_t *p_dec)
{
    m_xml_decorators.push_back(p_dec);
}
  

inline variable_idx_t ilp_problem_t::add_variable( const variable_t &var )
{
    variable_idx_t idx = static_cast<variable_idx_t>( m_variables.size() );
    m_variables.push_back( var );
    return idx;
}


inline constraint_idx_t
ilp_problem_t::add_constraint( const constraint_t &con )
{
    m_constraints.push_back( con );
    return m_constraints.size() - 1;
}


inline void ilp_problem_t::add_constancy_of_variable(
    variable_idx_t idx, double value)
{
    m_const_variable_values[idx] = value;
}


inline const hash_map<variable_idx_t, double>&
ilp_problem_t::const_variable_values() const
{
    return m_const_variable_values;
}


inline double ilp_problem_t::const_variable_value(variable_idx_t i) const
{
    return m_const_variable_values.at(i);
}


inline bool ilp_problem_t::is_constant_variable(variable_idx_t i) const
{
    return m_const_variable_values.count(i) > 0;
}


inline void ilp_problem_t::add_laziness_of_constraint(constraint_idx_t i)
{
    m_laziness_of_constraints.insert(i);
}


inline const hash_set<constraint_idx_t>&
ilp_problem_t::get_lazy_constraints() const
{
    return m_laziness_of_constraints;
}


inline const std::vector<variable_t>& ilp_problem_t::variables() const
{
    return m_variables;
}


inline const variable_t& ilp_problem_t::variable(variable_idx_t i) const
{
    return m_variables.at(i);
}


inline variable_t& ilp_problem_t::variable(variable_idx_t i)
{
    return m_variables[i];
}


inline const std::vector<constraint_t>& ilp_problem_t::constraints() const
{
    return m_constraints;
}


inline const constraint_t& ilp_problem_t::constraint(constraint_idx_t i) const
{
    return m_constraints.at(i);
}


inline constraint_t& ilp_problem_t::constraint(constraint_idx_t i)
{
    return m_constraints[i];
}


inline const pg::proof_graph_t* const ilp_problem_t::proof_graph() const
{
    return m_graph;
}


inline variable_idx_t ilp_problem_t::find_variable_with_node( pg::node_idx_t idx ) const
{
    if (idx < 0) return -1;
    auto it = m_map_node_to_variable.find(idx);
    return ( it != m_map_node_to_variable.end() ) ? it->second : -1;
}


inline variable_idx_t ilp_problem_t::find_variable_with_hypernode(
    pg::hypernode_idx_t idx ) const
{
    if (idx < 0) return -1;
    auto it = m_map_hypernode_to_variable.find(idx);
    return ( it != m_map_hypernode_to_variable.end() ) ? it->second : -1;
}


inline const hash_map<pg::node_idx_t, variable_idx_t>&
ilp_problem_t::node_to_variable() const
{
    return m_map_node_to_variable;
}


inline const hash_map<pg::hypernode_idx_t, variable_idx_t>&
ilp_problem_t::hypernode_to_variable() const
{
    return m_map_hypernode_to_variable;
}


inline void ilp_problem_t::add_attributes(
    const std::string &key, const std::string &value)
{
    m_attributes[key] = value;
}


inline bool ilp_problem_t::node_is_active(
    const ilp_solution_t &sol, pg::node_idx_t idx) const
{
    return m_solution_interpreter->node_is_active(sol, idx);
}


inline bool ilp_problem_t::hypernode_is_active(
    const ilp_solution_t &sol, pg::hypernode_idx_t idx) const
{
    return m_solution_interpreter->hypernode_is_active(sol, idx);
}


inline bool ilp_problem_t::edge_is_active(
    const ilp_solution_t &sol, pg::edge_idx_t idx) const
{
    return m_solution_interpreter->edge_is_active(sol, idx);
}


inline solution_type_e ilp_solution_t::type() const
{
    return m_solution_type;
}


inline double ilp_solution_t::value_of_objective_function() const
{
    return m_value_of_objective_function;
}




inline bool ilp_solution_t::variable_is_active( variable_idx_t idx ) const
{
    return (idx >= 0) ? (m_optimized_values.at(idx) > 0.5) : false;
}


inline bool ilp_solution_t::constraint_is_satisfied(constraint_idx_t idx) const
{
    return (idx >= 0) ? m_constraints_sufficiency.at(idx) : false;
}


} // end of ilp

} // end of phil


#endif

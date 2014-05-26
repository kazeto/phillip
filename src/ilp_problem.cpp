/* -*- coding: utf-8 -*- */


#include <sstream>
#include <set>

#include "./ilp_problem.h"
#include "./phillip.h"


namespace phil
{

namespace ilp
{


bool ilp_problem_t::ms_do_economize = true;


void constraint_t::print(
    std::string *p_out, const std::vector<variable_t> &var_instances ) const
{
    char buffer[10240];
    for( auto it=m_terms.begin(); it!=m_terms.end(); ++it )
    {
        if( it != m_terms.begin() )
            (*p_out) += " + ";
        const std::string& name = var_instances.at(it->var_idx).name();
        _sprintf( buffer, "%.2f * %s", it->coefficient, name.c_str() );
        (*p_out) += buffer;
    }
            
    switch( m_operator )
    {
    case OPR_EQUAL:
        _sprintf( buffer, " = %.2f", m_target[0] );
        (*p_out) += buffer;
        break;
    case OPR_LESS_EQ:
        _sprintf( buffer, " <= %.2f", m_target[0] );
        (*p_out) += buffer;
        break;
    case OPR_GREATER_EQ:
        _sprintf( buffer, " >= %.2f", m_target[0] );
        (*p_out) += buffer;
        break;
    case OPR_RANGE:
        _sprintf( buffer, ": %.2f ~ %.2f", m_target[0], m_target[1] );
        (*p_out) += buffer;
        break;
    }
}


ilp_problem_t::~ilp_problem_t()
{
    delete m_solution_interpreter;
    for (auto it = m_xml_decorators.begin(); it != m_xml_decorators.end(); ++it)
        delete *it;
}


variable_idx_t
    ilp_problem_t::add_variable_of_node( pg::node_idx_t idx, double coef )
{
    const pg::node_t &node = m_graph->node(idx);
    std::string lit = node.literal().to_string();
    variable_t var(format("n(%d):%s", idx, lit.c_str()), coef);
    variable_idx_t var_idx = add_variable(var);
    m_map_node_to_variable[idx] = var_idx;

    return var_idx;
}


variable_idx_t ilp_problem_t::add_variable_of_hypernode(
    pg::hypernode_idx_t idx, double coef, bool do_add_constraint_for_member)
{
    const std::vector<pg::node_idx_t> &hypernode = m_graph->hypernode(idx);
    if (hypernode.empty()) return -1;

    if (ms_do_economize)
    {
        /* IF A HYERNODE INCLUDE ONLY ONE LITERAL-NODE,
        * USE THE NODE'S VARIABLE AS THE HYPERNODE'S VARIABLE. */
        if (hypernode.size() == 1)
        {
            const pg::node_t &node = m_graph->node(hypernode.front());
            if (not node.is_equality_node() and not node.is_non_equality_node())
            {
                variable_idx_t var = find_variable_with_node(hypernode.front());
                if (var >= 0)
                {
                    m_map_hypernode_to_variable[idx] = var;
                    return var;
                }
            }
        }
    }

    std::string nodes =
        join(hypernode.begin(), hypernode.end(), "%d", ",");
    std::string name = format("hn(%d):n(%s)", idx, nodes.c_str());
    variable_idx_t var = add_variable(variable_t(name, coef));

    if (do_add_constraint_for_member)
    {
        /* FOR A HYPERNODE BEING TRUE, NODES IN IT MUST BE TRUE TOO.
        * IF ALL NODES IN A HYPERNODE ARE TRUE, THE HYPERNODE MUST BE TRUE TOO. */
        constraint_t cons(
            format("hn_n_dependency:hn(%d):n(%s)", idx, nodes.c_str()),
            OPR_RANGE, 0.0, 0.0);
        for (auto n = hypernode.begin(); n != hypernode.end(); ++n)
        {
            variable_idx_t v = find_variable_with_node(*n);
            if (v < 0) return -1;
            cons.add_term(v, 1.0);
        }

        cons.set_bound(0.0, 1.0 * (cons.terms().size() - 1));
        cons.add_term(var, -1.0 * cons.terms().size());
        add_constraint(cons);
    }

    m_map_hypernode_to_variable[idx] = var;
    return var;
}


constraint_idx_t ilp_problem_t::
add_constraint_of_dependence_of_node_on_hypernode(pg::node_idx_t idx)
{
    const pg::node_t &node = m_graph->node(idx);

    variable_idx_t var_node = find_variable_with_node(idx);
    if (var_node < 0) return -1;

    hash_set<pg::hypernode_idx_t> masters;
    if (not node.is_equality_node() and not node.is_non_equality_node())
    {
        if (node.master_hypernode() >= 0)
            masters.insert(node.master_hypernode());
    }
    else if (node.is_equality_node())
    {
        auto hns = m_graph->search_hypernodes_with_node(idx);
        if (hns != NULL)
        {
            hash_set<pg::edge_idx_t> parental_edges;
            for (auto it = hns->begin(); it != hns->end(); ++it)
                m_graph->enumerate_parental_edges(*it, &parental_edges);

            for (auto it = parental_edges.begin(); it != parental_edges.end(); ++it)
                masters.insert(m_graph->edge(*it).head());
        }
    }

    /* TO LET A LITERAL-NODE BE TRUE, ITS MASTER-HYPERNODE MUST BE TRUE */
    constraint_t con(format("n_dependency:n(%d)", idx), OPR_GREATER_EQ, 0.0);

    for (auto it = masters.begin(); it != masters.end(); ++it)
    {
        variable_idx_t var_master = find_variable_with_hypernode(*it);
        if (var_node == var_master)
        {
            assert(masters.size() == 1);
            return -1;
        }

        if (var_master >= 0)
            con.add_term(var_master, 1.0);
    }
    if (con.terms().empty()) return -1;

    con.add_term(var_node, -1.0);
    return add_constraint(con);
}



constraint_idx_t ilp_problem_t::
add_constraint_of_dependence_of_hypernode_on_parents(pg::hypernode_idx_t idx)
{
    variable_idx_t var = find_variable_with_hypernode(idx);
    if (var < 0) return -1;

    hash_set<pg::hypernode_idx_t> parents;
    m_graph->enumerate_parental_hypernodes(idx, &parents);
    if (parents.empty()) return -1;

    /* TO LET A HYPERNODE BE TRUE, ANY OF ITS PARENTS ARE MUST BE TRUE. */
    constraint_t con(format("hn_dependency:hn(%d)", idx), OPR_GREATER_EQ, 0.0);
    con.add_term(var, -1.0);
    for( auto hn = parents.begin(); hn != parents.end(); ++hn )
    {
        variable_idx_t v = find_variable_with_hypernode(*hn);
        if (v >= 0) con.add_term( v, 1.0 );
    }

    return add_constraint(con);
}


constraint_idx_t ilp_problem_t::add_constraint_of_mutual_exclusion(
    pg::node_idx_t n1, pg::node_idx_t n2, const pg::unifier_t &uni)
{
    std::string key = (n1 < n2) ?
        format("%d:%d", n1, n2) : format("%d:%d", n2, n1);

    /* IGNORE TUPLES WHICH HAVE BEEN CONSIDERED ALREADY. */
    if(m_log_of_node_tuple_for_mutual_exclusion.count(key) > 0)
        return -1;

    variable_idx_t var1 = find_variable_with_node(n1);
    variable_idx_t var2 = find_variable_with_node(n2);

    if( var1 < 0 or var2 < 0 ) return -1;

    /* N1 AND N2 CANNOT BE TRUE AT SAME TIME. */
    constraint_t con(
        format("inconsistency:n(%d,%d)", n1, n2), OPR_LESS_EQ, 1.0);
    con.add_term(var1, 1.0);
    con.add_term(var2, 1.0);

    bool f_fails = false;
    const std::set<literal_t> &subs = uni.substitutions();

    for (auto sub = subs.begin(); sub != subs.end(); ++sub)
    {
        const term_t &term1 = sub->terms[0];
        const term_t &term2 = sub->terms[1];
        if (term1.is_constant() and term2.is_constant() and term1 != term2)
            return -1;

        pg::node_idx_t sub_node = m_graph->find_sub_node(term1, term2);
        if (sub_node < 0) return -1;

        variable_idx_t sub_var = find_variable_with_node(sub_node);
        if (sub_var < 0) return -1;

        con.add_term(sub_var, 1.0);
        con.set_bound(con.bound() + 1.0);
    }

    m_log_of_node_tuple_for_mutual_exclusion.insert(key);

    return add_constraint(con);
}


void ilp_problem_t::add_constraints_of_mutual_exclusions()
{
    auto muexs = m_graph->enumerate_mutual_exclusive_nodes();

    for (auto it = muexs.begin(); it != muexs.end(); ++it)
    {
        add_constraint_of_mutual_exclusion(
            std::get<0>(*it), std::get<1>(*it), std::get<2>(*it));
    }
}


bool ilp_problem_t::add_constraints_of_transitive_unification(
    term_t t1, term_t t2, term_t t3)
{
    std::string key =
        format("%ld:%ld:%ld", t1.get_hash(), t2.get_hash(), t3.get_hash());

    /* IGNORE TRIPLETS WHICH HAVE BEEN CONSIDERED ALREADY. */
    if (m_log_of_term_triplet_for_transitive_unification.count(key) > 0)
        return false;

    pg::node_idx_t n_t1t2 = m_graph->find_sub_node(t1, t2);
    pg::node_idx_t n_t2t3 = m_graph->find_sub_node(t2, t3);
    pg::node_idx_t n_t3t1 = m_graph->find_sub_node(t3, t1);

    if (n_t1t2 < 0 or n_t2t3 < 0 or n_t3t1 < 0) return false;

    variable_idx_t v_t1t2 = find_variable_with_node(n_t1t2);
    variable_idx_t v_t2t3 = find_variable_with_node(n_t2t3);
    variable_idx_t v_t3t1 = find_variable_with_node(n_t3t1);

    if (v_t1t2 < 0 or v_t2t3 < 0 or v_t3t1 < 0) return false;
  
    std::string name1 =
        format("transitivity:(%s,%s,%s)",
        t1.string().c_str(), t2.string().c_str(), t3.string().c_str());
    constraint_t con_trans1(name1, OPR_GREATER_EQ, -1);
    con_trans1.add_term(v_t1t2, +1.0);
    con_trans1.add_term(v_t2t3, -1.0);
    con_trans1.add_term(v_t3t1, -1.0);

    std::string name2 =
        format("transitivity:(%s,%s,%s)",
        t2.string().c_str(), t3.string().c_str(), t1.string().c_str());
    constraint_t con_trans2(name2, OPR_GREATER_EQ, -1);
    con_trans2.add_term(v_t1t2, -1.0);
    con_trans2.add_term(v_t2t3, +1.0);
    con_trans2.add_term(v_t3t1, -1.0);

    std::string name3 =
        format("transitivity:(%s,%s,%s)",
        t3.string().c_str(), t1.string().c_str(), t2.string().c_str());
    constraint_t con_trans3(name3, OPR_GREATER_EQ, -1);
    con_trans3.add_term(v_t1t2, -1.0);
    con_trans3.add_term(v_t2t3, -1.0);
    con_trans3.add_term(v_t3t1, +1.0);

    constraint_idx_t idx_trans1 = add_constraint(con_trans1);
    constraint_idx_t idx_trans2 = add_constraint(con_trans2);
    constraint_idx_t idx_trans3 = add_constraint(con_trans3);

    // FOR CUTTING-PLANE
    add_laziness_of_constraint(idx_trans1);
    add_laziness_of_constraint(idx_trans2);
    add_laziness_of_constraint(idx_trans3);

    m_log_of_term_triplet_for_transitive_unification.insert(key);

    return 1;
}


void ilp_problem_t::add_constraints_of_transitive_unifications()
{
    std::list< const hash_set<term_t>* >
        clusters = m_graph->enumerate_variable_clusters();

    for( auto cl = clusters.begin(); cl != clusters.end(); ++cl )
    {
        if( (*cl)->size() <= 2 ) continue;

        std::vector<term_t> terms( (*cl)->begin(), (*cl)->end() );
        for( size_t i = 2; i < terms.size(); ++i )
        for( size_t j = 1; j < i;            ++j )
        for( size_t k = 0; k < j;            ++k )
        {
            add_constraints_of_transitive_unification(
                terms[i], terms[j], terms[k]);
        }
    }
}


void ilp_problem_t::
add_constrains_of_conditions_for_chain(pg::edge_idx_t idx)
{
    const pg::edge_t &edge = m_graph->edge(idx);
    variable_idx_t head = find_variable_with_hypernode(edge.head());
    if (head < 0) return;

    if (edge.type() != pg::EDGE_IMPLICATION and
        edge.type() != pg::EDGE_HYPOTHESIZE)
        return;

    hash_set<pg::node_idx_t> conds;
    bool is_available = m_graph->check_availability_of_chain(idx, &conds);

    // IF THE CHAIN IS NOT AVAILABLE, HEAD-HYPERNODE MUST BE FALSE.
    if (not is_available)
        add_constancy_of_variable(head, 0.0);
    else if (not conds.empty())
    {
        // TO PERFORM THE CHAINING, NODES IN conds MUST BE TRUE.
        constraint_t con(
            format("condition_for_chain:e(%d)", idx), OPR_GREATER_EQ, 0.0);

        for (auto n = conds.begin(); n != conds.end(); ++n)
        {
            variable_idx_t _v = find_variable_with_node(*n);
            assert(_v >= 0);
            con.add_term(_v, 1.0);
        }

        con.add_term(head, -1.0 * con.terms().size());
        add_constraint(con);
    }
}


void ilp_problem_t::add_constrains_of_exclusive_chains()
{
    IF_VERBOSE_4("Adding constraints of exclusiveness of chains...");

    auto excs = m_graph->enumerate_mutual_exclusive_edges();
    int num = add_constrains_of_exclusive_chains(excs);

    IF_VERBOSE_4(format("    # of added constraints = %d", num));
}


size_t ilp_problem_t::add_constrains_of_exclusive_chains(
    const std::list< hash_set<pg::edge_idx_t> > &exc)
{
    size_t num_of_added_constraints(0);

    for (auto it = exc.begin(); it != exc.end(); ++it)
    {
        std::string name =
            "exclusive_chains(" +
            join(it->begin(), it->end(), "%d", ",") + ")";
        constraint_t con(name, OPR_GREATER_EQ, -1.0);

        for (auto e = it->begin(); e != it->end(); ++e)
        {
            pg::hypernode_idx_t hn = m_graph->edge(*e).head();
            variable_idx_t v = find_variable_with_hypernode(hn);
            if (v >= 0)
                con.add_term(v, -1.0);
            else
                break;
        }

        if (con.terms().size() == it->size())
        {
            add_constraint(con);
            ++num_of_added_constraints;
        }
    }

    return num_of_added_constraints;
}


template<class T> variable_idx_t
    ilp_problem_t::find_variable_with_hypernode_unordered(T begin, T end) const
{
    const hash_set<pg::hypernode_idx_t> *hns =
        m_graph->find_hypernode_with_unordered_nodes(begin, end);
    if (hns == NULL)
        return -1;
    else
    {
        for (auto it = hns->begin(); it != hns->end(); ++it)
        {
            variable_idx_t i = find_variable_with_hypernode(*it);
            if (i >= 0) return i;
        }
    }
    return -1;
}


double ilp_problem_t::get_value_of_objective_function(
    const std::vector<double> &values) const
{
    double out(0.0);
    for (variable_idx_t i=0; i<m_variables.size(); ++i)
        out += values.at(i) * m_variables.at(i).objective_coefficient();
    return out;
}


void ilp_problem_t::print(std::ostream *os) const
{
    (*os)
        << "<ilp name=\"" << name()
        << "\" maxmize=\"" << (do_maximize() ? "yes" : "no")
        << "\" time=\"" << sys()->get_time_for_ilp()
        << "\" timeout=\"" << (is_timeout() ? "yes" : "no");

    for (auto attr = m_attributes.begin(); attr != m_attributes.end(); ++attr)
        (*os) << "\" " << attr->first << "=\"" << attr->second;

    (*os)
        << "\">" << std::endl
        << "<variables num=\"" << m_variables.size() << "\">" << std::endl;
    
    for (int i = 0; i < m_variables.size(); i++)
    {
        const variable_t &var = m_variables.at(i);
        (*os) << "<variable name=\"" << var.name()
              << "\" coefficient=\"" << var.objective_coefficient() << "\"";
        if (is_constant_variable(i))
            (*os) << " fixed=\"" << const_variable_values().at(i) << "\"";
        (*os) << " />" << std::endl;
    }
    
    (*os)
        << "</variables>" << std::endl
        << "<constraints num=\"" << m_constraints.size()
        << "\">" << std::endl;

    for (int i = 0; i < m_constraints.size(); i++)
    {
        const constraint_t &cons = m_constraints.at(i);
        std::string cons_exp;
        cons.print(&cons_exp, m_variables);
        (*os) << "<constraint name=\"" << cons.name()
              << "\">" << cons_exp << "</constraint>" << std::endl;
    }
    
    (*os) << "</constraints>" << std::endl
          << "</ilp>" << std::endl;
}


void ilp_problem_t::print_solution(
    const ilp_solution_t *sol, std::ostream *os) const
{
    std::string state;
    switch (sol->type())
    {
    case ilp::SOLUTION_OPTIMAL: state = "optimal"; break;
    case ilp::SOLUTION_SUB_OPTIMAL: state = "sub-optimal"; break;
    case ilp::SOLUTION_NOT_AVAILABLE: state = "not-available"; break;
    }
    assert(not state.empty());

    (*os)
        << "<proofgraph name=\"" << name()
        << "\" state=\"" << state
        << "\" objective=\"" << sol->value_of_objective_function()
        << "\">" << std::endl;

    (*os)
        << "<time lhs=\"" << sys()->get_time_for_lhs()
        << "\" ilp=\"" << sys()->get_time_for_ilp()
        << "\" sol=\"" << sys()->get_time_for_sol()
        << "\" all=\"" << sys()->get_time_for_infer()
        << "\"></time>" << std::endl;

    const ilp::ilp_problem_t *prob(sol->problem());
    const pg::proof_graph_t *graph(sol->problem()->proof_graph());
    bool is_time_out_all =
        graph->is_timeout() or prob->is_timeout() or sol->is_timeout();
    (*os)
        << "<timeout lhs=\"" << (graph->is_timeout() ? "yes" : "no")
        << "\" ilp=\"" << (prob->is_timeout() ? "yes" : "no")
        << "\" sol=\"" << (sol->is_timeout() ? "yes" : "no")
        << "\" all=\"" << (is_time_out_all ? "yes" : "no")
        << "\"></timeout>" << std::endl;

    _print_literals_in_solution(sol, os);
    _print_explanations_in_solution(sol, os);
    _print_unifications_in_solution(sol, os);
    
    (*os) << "</proofgraph>" << std::endl;
}


void ilp_problem_t::_print_literals_in_solution(
    const ilp_solution_t *sol, std::ostream *os) const
{
    std::list<pg::node_idx_t> indices;
    for (int i = 0; i < m_graph->nodes().size(); ++i)
    {
        const pg::node_t &node = m_graph->node(i);
        if (not node.is_equality_node() and not node.is_non_equality_node())
            indices.push_back(i);
    }

    (*os) << "<literals num=\"" << indices.size() << "\">" << std::endl;

    for (auto it = indices.begin(); it != indices.end(); ++it)
    {
        pg::node_idx_t n_idx = (*it);
        const pg::node_t &node = m_graph->node(n_idx);
        bool is_active = node_is_active(*sol, n_idx);
        std::string type;

        switch (node.type())
        {
        case pg::NODE_UNDERSPECIFIED: type = "underspecified"; break;
        case pg::NODE_OBSERVABLE:     type = "observable";     break;
        case pg::NODE_HYPOTHESIS:     type = "hypothesis";     break;
        case pg::NODE_LABEL:          type = "label";          break;
        }

        (*os)
            << "<literal id=\"" << n_idx
            << "\" type=\"" << type
            << "\" depth=\"" << node.depth()
            << "\" active=\"" << (is_active ? "yes" : "no");

        hash_map<std::string, std::string> attributes;
        for (auto dec = m_xml_decorators.begin(); dec != m_xml_decorators.end(); ++dec)
            (*dec)->get_literal_attributes(sol, n_idx, &attributes);
        for (auto attr = attributes.begin(); attr != attributes.end(); ++attr)
            (*os) << "\" " << attr->first << "=\"" << attr->second;

        (*os)
            << "\">" << node.to_string() << "</literal>" << std::endl;
    }

    (*os) << "</literals>" << std::endl;
}


void ilp_problem_t::_print_explanations_in_solution(
    const ilp_solution_t *sol, std::ostream *os) const
{
    const kb::knowledge_base_t *base = sys()->knowledge_base();
    std::list<pg::edge_idx_t> indices;

    for (int i = 0; i < m_graph->edges().size(); ++i)
    if (m_graph->edge(i).is_chain_edge())
        indices.push_back(i);

    (*os) << "<explanations num=\"" << indices.size() << "\">" << std::endl;

    for (auto it = indices.begin(); it != indices.end(); ++it)
    {
        const pg::edge_t &edge = m_graph->edge(*it);
        const std::vector<pg::node_idx_t>
            &hn_from(m_graph->hypernode(edge.tail())),
            &hn_to(m_graph->hypernode(edge.head()));
        bool is_backward = (edge.type() == pg::EDGE_HYPOTHESIZE);
        std::string
            s_from(join(hn_from.begin(), hn_from.end(), "%d", ",")),
            s_to(join(hn_to.begin(), hn_to.end(), "%d", ",")),
            axiom_name = "_blank";

        if (edge.axiom_id() >= 0)
            axiom_name = base->get_axiom(edge.axiom_id()).name;

        (*os)
            << "<explanation id=\"" << (*it)
            << "\" tail=\"" << m_graph->hypernode2str(edge.tail())
            << "\" head=\"" << m_graph->hypernode2str(edge.head())
            << "\" active=\"" << (edge_is_active(*sol, *it) ? "yes" : "no")
            << "\" backward=\"" << (is_backward ? "yes" : "no")
            << "\" axiom=\"" << axiom_name;

        hash_map<std::string, std::string> attributes;
        for (auto dec = m_xml_decorators.begin(); dec != m_xml_decorators.end(); ++dec)
            (*dec)->get_explanation_attributes(sol, *it, &attributes);
        for (auto attr = attributes.begin(); attr != attributes.end(); ++attr)
            (*os) << "\" " << attr->first << "=\"" << attr->second;

        (*os)
            << "\">" << m_graph->edge_to_string(*it)
            << "</explanation>" << std::endl;
    }

    (*os) << "</explanations>" << std::endl;
}


void ilp_problem_t::_print_unifications_in_solution(
    const ilp_solution_t *sol, std::ostream *os) const
{
    std::list<pg::edge_idx_t> indices;
    for (int i = 0; i < m_graph->edges().size(); ++i)
    if (m_graph->edge(i).is_unify_edge())
        indices.push_back(i);

    (*os) << "<unifications num=\"" << indices.size() << "\">" << std::endl;

    for (auto it = indices.begin(); it != indices.end(); ++it)
    {
        const pg::edge_t& edge = m_graph->edge(*it);
        std::vector<std::string> subs;

        if (edge.head() >= 0)
        {
            const std::vector<pg::node_idx_t>
                &hn_to(m_graph->hypernode(edge.head()));
            for (auto it = hn_to.begin(); it != hn_to.end(); ++it)
            {
                const literal_t &lit = m_graph->node(*it).literal();
                assert(lit.predicate == "=");
                subs.push_back(
                    lit.terms[0].string() + "=" + lit.terms[1].string());
            }
        }

        const std::vector<pg::node_idx_t>
            &hn_from(m_graph->hypernode(edge.tail()));
        (*os)
            << "<unification l1=\"" << hn_from[0]
            << "\" l2=\"" << hn_from[1]
            << "\" unifier=\"" << join(subs.begin(), subs.end(), ", ")
            << "\" active=\"" << (edge_is_active(*sol, *it) ? "yes" : "no");

        hash_map<std::string, std::string> attributes;
        for (auto dec = m_xml_decorators.begin(); dec != m_xml_decorators.end(); ++dec)
            (*dec)->get_unification_attributes(sol, *it, &attributes);
        for (auto attr = attributes.begin(); attr != attributes.end(); ++attr)
            (*os) << "\" " << attr->first << "=\"" << attr->second;

        (*os)
            << "\">"
            << m_graph->edge_to_string(*it)
            << "</unification>" << std::endl;
    }

    (*os) << "</unifications>" << std::endl;
}


ilp_solution_t::ilp_solution_t(
    const ilp_problem_t *prob, solution_type_e sol_type,
    const std::vector<double> &values, const std::string &name)
    : m_name(name), m_ilp(prob), m_solution_type(sol_type),
      m_optimized_values(values),
      m_constraints_sufficiency(prob->constraints().size(), false),
      m_value_of_objective_function(prob->get_value_of_objective_function(values)),
      m_is_timeout(false)
{
    for (int i = 0; i < prob->constraints().size(); ++i)
    {
        const constraint_t &cons = prob->constraint(i);
        m_constraints_sufficiency[i] = cons.is_satisfied(values);
    }
}


void ilp_solution_t::filter_unsatisfied_constraints(
    hash_set<constraint_idx_t> *targets,
    hash_set<constraint_idx_t> *filtered) const
{
    for (auto it = targets->begin(); it != targets->end();)
    {
        if (not constraint_is_satisfied(*it))
        {
            filtered->insert(*it);
            it = targets->erase(it);
        }
        else
            ++it;
    }
}


std::string ilp_solution_t::to_string() const
{
    std::ostringstream exp;
    print(&exp);
    return exp.str();
}


void ilp_solution_t::print(std::ostream *os) const
{
    (*os)
        << "<solution name=\"" << name()
        << "\" time=\"" << sys()->get_time_for_sol()
        << "\" timeout=\"" << (is_timeout() ? "yes" : "no")
        << "\">" << std::endl
        << "<variables num=\"" << m_ilp->variables().size()
        << "\">" << std::endl;

    for( int i=0; i<m_ilp->variables().size(); ++i )
    {
        const variable_t& var = m_ilp->variable(i);
        (*os) << "<variable name=\"" << var.name()
              << "\" coefficient=\""<< var.objective_coefficient()
              << "\">"<< m_optimized_values[i] <<"</variable>" << std::endl;
    }

    (*os) << "</variables>" << std::endl
          << "<constraints num=\"" << m_ilp->constraints().size()
          << "\">" << std::endl;

    for( int i=0; i<m_ilp->constraints().size(); i++ )
    {
        const constraint_t &cons = m_ilp->constraint(i);
        (*os) << "<constraint name=\"" << cons.name() << "\">"
              << (m_constraints_sufficiency.at(i) ? "1" : "0")
              << "</constraint>" << std::endl;
    }

    (*os) << "</constraints>" << std::endl
          << "</solution>" << std::endl;
}


void ilp_solution_t::print_graph(std::ostream *os) const
{
    m_ilp->print_solution(this, os);
}



bool basic_solution_interpreter_t::node_is_active(
    const ilp_solution_t &sol, pg::node_idx_t idx) const
{
    variable_idx_t var = sol.problem()->find_variable_with_node(idx);
    return (var >= 0) ? sol.variable_is_active(var) : false;
}


bool basic_solution_interpreter_t::hypernode_is_active(
    const ilp_solution_t &sol, pg::hypernode_idx_t idx) const
{
    variable_idx_t var = sol.problem()->find_variable_with_hypernode(idx);
    return (var >= 0) ? sol.variable_is_active(var) : false;
}


bool basic_solution_interpreter_t::edge_is_active(
    const ilp_solution_t &sol, pg::edge_idx_t idx) const
{
    const pg::edge_t &edge = sol.problem()->proof_graph()->edge(idx);
    return
        hypernode_is_active(sol, edge.tail()) and
        hypernode_is_active(sol, edge.head());
}


}

}

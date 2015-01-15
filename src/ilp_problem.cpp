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


void ilp_problem_t::merge(const ilp_problem_t &prob)
{
#define foreach(it, con) for(auto it = con.begin(); it != con.end(); ++it)
    int num_v(m_variables.size());
    int num_c(m_constraints.size());
    int num_n(m_graph->nodes().size());
    int num_hn(m_graph->hypernodes().size());
    int num_e(m_graph->edges().size());

    m_do_maximize = prob.m_do_maximize;
    m_is_timeout = (m_is_timeout or prob.m_is_timeout);

    foreach (it, prob.m_variables)
        m_variables.push_back(*it);

    foreach(it, prob.m_constraints)
    {
        constraint_t con(*it);
        for (int i = 0; i < con.terms().size(); ++i)
            con.term(i).var_idx += num_v;
        m_constraints.push_back(con);
    }

    foreach(it, prob.m_const_variable_values)
        m_const_variable_values[it->first + num_v] = it->second;

    foreach(it, prob.m_laziness_of_constraints)
        m_laziness_of_constraints.insert((*it) + num_c);

    foreach(it, prob.m_map_node_to_variable)
        m_map_node_to_variable[it->first + num_n] = it->second + num_v;

    foreach(it, prob.m_map_hypernode_to_variable)
        m_map_hypernode_to_variable[it->first + num_hn] = it->second + num_v;

    m_log_of_term_triplet_for_transitive_unification.insert(
        prob.m_log_of_term_triplet_for_transitive_unification.begin(),
        prob.m_log_of_term_triplet_for_transitive_unification.end());
    m_log_of_node_tuple_for_mutual_exclusion.insert(
        prob.m_log_of_node_tuple_for_mutual_exclusion.begin(),
        prob.m_log_of_node_tuple_for_mutual_exclusion.end());

    foreach(it, prob.m_variables_for_requirements)
        m_variables_for_requirements.push_back(
        std::make_pair(it->first, it->second + num_v));

    // SKIP TO MERGE m_log_of_*
#undef foreach
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
        /* FOR A HYPERNODE BEING TRUE, ITS ALL MEMBERS MUST BE TRUE TOO. */
        constraint_t cons(
            format("hn_n_dependency:hn(%d):n(%s)", idx, nodes.c_str()),
            OPR_GREATER_EQ, 0.0);
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


variable_idx_t ilp_problem_t::add_variable_of_edge(
    pg::edge_idx_t idx, double coef, bool do_add_constraint_for_node)
{
    const pg::edge_t &edge = m_graph->edge(idx);

    if (ms_do_economize)
    {
        variable_idx_t var(-1);

        if (edge.is_chain_edge())
            variable_idx_t var = find_variable_with_hypernode(edge.head());
        if (edge.is_unify_edge() and edge.head() < 0)
            variable_idx_t var = find_variable_with_hypernode(edge.tail());

        if (var >= 0)
        {
            m_map_edge_to_variable[idx] = var;
            return var;
        }
    }

    variable_idx_t var = add_variable(variable_t(
        format("edge(%d):hn(%d,%d)", idx, edge.tail(), edge.head()), 0.0));

    if (do_add_constraint_for_node)
    {
        /* IF THE EDGE IS TRUE, TAIL AND HEAD MUST BE TRUE, TOO. */
        constraint_t con(
            format("e_hn_dependency:e(%d):hn(%d,%d)", idx, edge.tail(), edge.head()),
            OPR_GREATER_EQ, 0.0);
        variable_idx_t v_tail = find_variable_with_hypernode(edge.tail());
        variable_idx_t v_head = find_variable_with_hypernode(edge.head());

        if (v_tail >= 0 and (v_head >= 0 or edge.head() < 0))
        {
            con.add_term(v_tail, 1.0);
            if (edge.head() >= 0)
                con.add_term(v_head, 1.0);
            con.add_term(var, -1.0 * con.terms().size());
            add_constraint(con);
        }
    }

    m_map_edge_to_variable[idx] = var;
    return var;
}


constraint_idx_t ilp_problem_t::
add_constraint_of_dependence_of_node_on_hypernode(pg::node_idx_t idx)
{   
    const pg::node_t &node = m_graph->node(idx);
    if (node.is_equality_node() or node.is_non_equality_node()) return -1;

    variable_idx_t var_node = find_variable_with_node(idx);
    if (var_node < 0) return -1;

    hash_set<pg::hypernode_idx_t> masters;
    if (node.is_equality_node() or node.is_non_equality_node())
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
    else if (node.master_hypernode() >= 0)
        masters.insert(node.master_hypernode());

    /* TO LET A NODE BE TRUE, ITS MASTER-HYPERNODES IS TRUE */
    constraint_t con(format("n_dependency:n(%d)", idx), OPR_GREATER_EQ, 0.0);

    for (auto it = masters.begin(); it != masters.end(); ++it)
    {
        variable_idx_t var_master = find_variable_with_hypernode(*it);

        if (var_node != var_master and var_master >= 0)
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


void ilp_problem_t::add_constraints_to_forbid_chaining_from_explained_node(
    pg::edge_idx_t idx_unify, pg::node_idx_t idx_explained,
    std::list<constraint_idx_t> *out)
{
    const pg::edge_t &e_uni = m_graph->edge(idx_unify);
    if (not e_uni.is_unify_edge()) return;

    // IF A LITERAL IS UNIFIED AND EXPLAINED BY ANOTHER ONE,
    // THEN CHAINING FROM THE LITERAL IS FORBIDDEN.

    variable_idx_t v_uni_tail = find_variable_with_hypernode(e_uni.tail());
    variable_idx_t v_uni_head = find_variable_with_hypernode(e_uni.head());
    variable_idx_t v_uni = (e_uni.head() >= 0 ? v_uni_head : v_uni_tail);
    if (v_uni_tail < 0 or(e_uni.head() >= 0 and v_uni_head < 0)) return;

    auto from = m_graph->hypernode(e_uni.tail());
    if (from[0] != idx_explained and from[1] != idx_explained) return;

    auto hns = m_graph->search_hypernodes_with_node(idx_explained);
    for (auto hn = hns->begin(); hn != hns->end(); ++hn)
    {
        auto es = m_graph->search_edges_with_hypernode(*hn);
        for (auto j = es->begin(); j != es->end(); ++j)
        {
            const pg::edge_t &e_ch = m_graph->edge(*j);
            if (not e_ch.is_chain_edge()) continue;
            if (e_ch.tail() != (*hn)) continue;

            constraint_t con(
                format("unify_or_chain:e(%d):e(%d)", idx_unify, *j), OPR_GREATER_EQ, -1.0);
            variable_idx_t v_ch_head = find_variable_with_hypernode(e_ch.head());

            if (v_ch_head >= 0)
            {
                con.add_term(v_ch_head, -1.0);
                con.add_term(v_uni_tail, -1.0);
                if (e_uni.head() >= 0)
                {
                    con.add_term(v_uni_head, -1.0);
                    con.set_bound(con.bound() - 1.0);
                }

                constraint_idx_t _idx = add_constraint(con);

                if (_idx >= 0 and out != NULL)
                    out->push_back(_idx);
            }
        }
    }
}


void ilp_problem_t::add_constraints_to_forbid_looping_unification(
    pg::edge_idx_t idx_unify, pg::node_idx_t idx_explained,
    std::list<constraint_idx_t> *out)
{
    const pg::edge_t &e_uni = m_graph->edge(idx_unify);
    if (not e_uni.is_unify_edge()) return;

    // IF A LITERAL IS UNIFIED AND EXPLAINED BY ANOTHER ONE,
    // THEN CHAINING FROM THE LITERAL IS FORBIDDEN.

    variable_idx_t v_uni_tail = find_variable_with_hypernode(e_uni.tail());
    variable_idx_t v_uni_head = find_variable_with_hypernode(e_uni.head());
    if (v_uni_tail < 0 or(e_uni.head() >= 0 and v_uni_head < 0)) return;

    auto from = m_graph->hypernode(e_uni.tail());
    if (from[0] != idx_explained and from[1] != idx_explained) return;

    pg::node_idx_t idx_explains = (from[0] == idx_explained) ? from[1] : from[0];
    hash_set<pg::node_idx_t> descendants;
    hash_set<pg::node_idx_t> ancestors(m_graph->node(idx_explained).evidences());

    m_graph->enumerate_descendant_nodes(idx_explains, &descendants);
    descendants.insert(idx_explains);
    ancestors.insert(idx_explained);

    hash_map<std::string, hash_set<pg::node_idx_t> > a2n_1, a2n_2;
    for (auto it = descendants.begin(); it != descendants.end(); ++it)
        a2n_1[m_graph->node(*it).literal().get_arity()].insert(*it);
    for (auto it = ancestors.begin(); it != ancestors.end(); ++it)
        a2n_2[m_graph->node(*it).literal().get_arity()].insert(*it);

    for (auto it1 = a2n_1.begin(); it1 != a2n_1.end(); ++it1)
    {
        auto it2 = a2n_2.find(it1->first);
        if (it2 == a2n_2.end()) continue;

        for (auto n1 = it1->second.begin(); n1 != it1->second.end(); ++n1)
        for (auto n2 = it2->second.begin(); n2 != it2->second.end(); ++n2)
        {
            pg::edge_idx_t _uni = m_graph->find_unifying_edge(*n1, *n2);
            if (_uni < 0 or _uni == idx_unify) continue;

            const pg::edge_t &_e_uni = m_graph->edge(_uni);
            ilp::variable_idx_t _v_uni_tail = find_variable_with_hypernode(_e_uni.tail());
            ilp::variable_idx_t _v_uni_head = find_variable_with_hypernode(_e_uni.head());

            if (_v_uni_tail >= 0 and(_e_uni.head() < 0 or _v_uni_head >= 0))
            {
                ilp::constraint_t con(
                    format("muex_unify:e(%d,%d)", idx_unify, _uni),
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

                constraint_idx_t _idx = add_constraint(con);

                if (_idx >= 0 and out != NULL)
                    out->push_back(_idx);
            }
        }
    }
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


void ilp_problem_t::add_variable_for_requirement(
    const lf::logical_function_t &req, bool do_maximize)
{
    if (not req.is_operator(lf::OPR_REQUIREMENT)) return;

    const double PENALTY = do_maximize ? -10000.0 : 10000.0;
    typedef std::pair<const lf::logical_function_t*, hash_set<variable_idx_t> > _requirement_t;
    std::list<_requirement_t> requirements;

    /// Returns variables to be true in order to satisfy the requirement.
    auto enumerate_required_node = [&](const literal_t &lit) -> hash_set<variable_idx_t>
    {
        hash_set<pg::node_idx_t> ns = m_graph->enumerate_nodes_with_literal(lit);
        hash_set<variable_idx_t> out;
        pg::node_idx_t n_req(-1);

        for (auto it = ns.begin(); it != ns.end(); ++it)
        {
            const pg::node_t &n = m_graph->node(*it);
            if (n.type() == pg::NODE_REQUIRED or lit.is_equality())
            {
                n_req = *it;
                break;
            }
        }

        if (lit.is_equality())
        {
            variable_idx_t v = find_variable_with_node(n_req);
            if (v >= 0) out.insert(v);
        }
        else
        {
            const hash_set<pg::node_idx_t> *_nodes =
                m_graph->search_nodes_with_arity(lit.get_arity());
            hash_set<pg::hypernode_idx_t> _hns;

            for (auto it = _nodes->begin(); it != _nodes->end(); ++it)
            {
                pg::edge_idx_t e = m_graph->find_unifying_edge(n_req, *it);
                if (e >= 0) _hns.insert(m_graph->edge(e).head());
            }

            for (auto it = _hns.begin(); it != _hns.end(); ++it)
            {
                variable_idx_t v = find_variable_with_hypernode(*it);
                if (v >= 0) out.insert(v);
            }
        }

        return out;
    };

    auto add_requirement_lit = [&](
        const lf::logical_function_t &_lf, _requirement_t *out)
    {
        hash_set<variable_idx_t> _vars = enumerate_required_node(_lf.literal());
        if (not _vars.empty())
            out->second.insert(_vars.begin(), _vars.end());
    };

    auto add_requirement_or = [&](const lf::logical_function_t &_lf)
    {
        _requirement_t _or = std::make_pair(&_lf, hash_set<variable_idx_t>());
        for (auto it_br = _lf.branches().begin(); it_br != _lf.branches().end(); ++it_br)
        {
            if (it_br->is_operator(lf::OPR_LITERAL))
                add_requirement_lit(it_br->literal(), &_or);
        }
        if (not _or.second.empty())
            requirements.push_back(_or);
    };

    for (auto it = req.branches().begin(); it != req.branches().end(); ++it)
    {
        if (it->is_operator(lf::OPR_LITERAL))
        {
            _requirement_t _req = std::make_pair(&(*it), hash_set<variable_idx_t>());
            add_requirement_lit(*it, &_req);
            if (not _req.second.empty()) requirements.push_back(_req);
        }
        else if (it->is_operator(lf::OPR_OR))
            add_requirement_or(*it);
    }

    // ADDING VARIABLES & CONSTRAINTS.
    for (auto it = requirements.begin(); it != requirements.end(); ++it)
    {
        std::string str = it->first->to_string();
        variable_idx_t var = add_variable(
            variable_t(format("violation:%s", str.c_str()), PENALTY));
        constraint_t con(
            format("for_requirement:%s", str.c_str()), OPR_GREATER_EQ, 1.0);

        for (auto v = it->second.begin(); v != it->second.end(); ++v)
            con.add_term(*v, 1.0);
        con.add_term(var, 1.0);

        add_constraint(con);
        m_variables_for_requirements.push_back(std::make_pair(it->first, var));
    }
}


void ilp_problem_t::
add_constrains_of_conditions_for_chain(pg::edge_idx_t idx)
{
    const pg::edge_t &edge = m_graph->edge(idx);
    variable_idx_t v_edge = find_variable_with_edge(idx);
    if (v_edge < 0) return;

    if (not edge.is_chain_edge())
        return;

    hash_set<pg::node_idx_t> conds1, conds2;
    bool is_available = m_graph->check_availability_of_chain(idx, &conds1, &conds2);

    // IF THE CHAIN IS NOT AVAILABLE, HEAD-HYPERNODE MUST BE FALSE.
    if (not is_available)
        add_constancy_of_variable(v_edge, 0.0);
    else
    {
        if (not conds1.empty())
        {
            // TO PERFORM THE CHAINING, NODES IN conds1 MUST BE TRUE.
            constraint_t con(
                format("node_must_be_true_for_chain:e(%d)", idx),
                OPR_GREATER_EQ, 0.0);

            for (auto n = conds1.begin(); n != conds1.end(); ++n)
            {
                variable_idx_t _v = find_variable_with_node(*n);
                assert(_v >= 0);
                con.add_term(_v, 1.0);
            }

            con.add_term(v_edge, -1.0 * con.terms().size());
            add_constraint(con);
        }

        if (not conds2.empty())
        {
            // TO PERFORM THE CHAINING, NODES IN conds2 MUST NOT BE TRUE.
            constraint_t con(
                format("node_must_be_false_for_chain:e(%d)", idx),
                OPR_GREATER_EQ, 0.0);

            for (auto n = conds2.begin(); n != conds2.end(); ++n)
            {
                variable_idx_t _v = find_variable_with_node(*n);
                assert(_v >= 0);
                con.add_term(_v, -1.0);
            }

            double b = -1.0 * con.terms().size();
            con.add_term(v_edge, b);
            con.set_bound(b);
            add_constraint(con);
        }
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
        << "\" time=\"" << phillip()->get_time_for_ilp()
        << "\" timeout=\"" << (is_timeout() ? "yes" : "no");

    for (auto attr = m_attributes.begin(); attr != m_attributes.end(); ++attr)
        (*os) << "\" " << attr->first << "=\"" << attr->second;

    (*os)
        << "\">" << std::endl
        << "<variables num=\"" << m_variables.size() << "\">" << std::endl;
    
    for (int i = 0; i < m_variables.size(); i++)
    {
        const variable_t &var = m_variables.at(i);
        (*os) << "<variable index=\"" << i
              << "\" name=\"" << var.name()
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
        (*os) << "<constraint index=\"" << i
              << "\" name=\"" << cons.name()
              << "\">" << cons_exp << "</constraint>" << std::endl;
    }
    
    (*os) << "</constraints>" << std::endl
          << "</ilp>" << std::endl;
}


void ilp_problem_t::print_solution(
    const ilp_solution_t *sol, std::ostream *os) const
{
    if (os == &std::cout)
        g_mutex_for_print.lock();

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
        << "<time lhs=\"" << phillip()->get_time_for_lhs()
        << "\" ilp=\"" << phillip()->get_time_for_ilp()
        << "\" sol=\"" << phillip()->get_time_for_sol()
        << "\" all=\"" << phillip()->get_time_for_infer()
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

    _print_requirements_in_solution(sol, os);
    _print_literals_in_solution(sol, os);
    _print_explanations_in_solution(sol, os);
    _print_unifications_in_solution(sol, os);
    
    (*os) << "</proofgraph>" << std::endl;

    if (os == &std::cout)
        g_mutex_for_print.unlock();
}


void ilp_problem_t::_print_requirements_in_solution(
    const ilp_solution_t *sol, std::ostream *os) const
{
    const std::list< std::pair<const lf::logical_function_t*, variable_idx_t> >
        &vars = m_variables_for_requirements;

    (*os) << "<requirements num=\"" << vars.size() << "\">" << std::endl;

    for (auto it = vars.begin(); it != vars.end(); ++it)
    {
        std::string lf_str = it->first->to_string();
        bool is_satisfied = not sol->variable_is_active(it->second);
        (*os)
            << "<requirement satisfied=\"" << (is_satisfied ? "yes" : "no")
            << "\">" << lf_str << "</requirement>" << std::endl;
    }

    (*os) << "</requirements>" << std::endl;
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
        case pg::NODE_REQUIRED:       type = "requirement";    break;
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
    const kb::knowledge_base_t *base = kb::knowledge_base_t::instance();
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
                // assert(lit.predicate == "=");
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


void ilp_solution_t::merge(const ilp_solution_t &sol)
{
    m_solution_type =
        ((int)m_solution_type > (int)sol.m_solution_type) ?
        m_solution_type : sol.m_solution_type;
    
    m_optimized_values.insert(
        m_optimized_values.end(),
        sol.m_optimized_values.begin(),
        sol.m_optimized_values.end());
    m_constraints_sufficiency.insert(
        m_constraints_sufficiency.end(),
        sol.m_constraints_sufficiency.begin(),
        sol.m_constraints_sufficiency.end());
    m_value_of_objective_function += sol.m_value_of_objective_function;

    m_is_timeout = (m_is_timeout or sol.m_is_timeout);
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
        << "\" time=\"" << phillip()->get_time_for_sol()
        << "\" timeout=\"" << (is_timeout() ? "yes" : "no")
        << "\">" << std::endl
        << "<variables num=\"" << m_ilp->variables().size()
        << "\">" << std::endl;

    for( int i=0; i<m_ilp->variables().size(); ++i )
    {
        const variable_t& var = m_ilp->variable(i);
        (*os) << "<variable index=\"" << i
              << "\" name=\"" << var.name()
              << "\" coefficient=\""<< var.objective_coefficient()
              << "\">"<< m_optimized_values[i] <<"</variable>" << std::endl;
    }

    (*os) << "</variables>" << std::endl
          << "<constraints num=\"" << m_ilp->constraints().size()
          << "\">" << std::endl;

    for( int i=0; i<m_ilp->constraints().size(); i++ )
    {
        const constraint_t &cons = m_ilp->constraint(i);
        (*os) << "<constraint index=\"" << i
              << "\" name=\"" << cons.name() << "\">"
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

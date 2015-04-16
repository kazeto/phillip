/* -*- coding: utf-8 -*- */

#include <algorithm>
#include <functional>
#include <cassert>
#include <set>

#include "./proof_graph.h"
#include "./phillip.h"


namespace phil
{

namespace pg
{



node_t::node_t(
    const proof_graph_t *graph,
    const literal_t &lit, node_type_e type, node_idx_t idx,
    depth_t depth, const hash_set<node_idx_t> &parents)
    : m_type(type), m_literal(lit), m_index(idx),
    m_depth(depth), m_arity_id(kb::INVALID_ARITY_ID),
    m_master_hypernode_idx(-1), m_parents(parents), m_ancestors(parents)
{
    for (auto idx : parents)
    {
        const hash_set<node_idx_t> &nodes = graph->node(idx).ancestors();
        m_ancestors.insert(nodes.begin(), nodes.end());
    }

    if (not m_literal.is_equality())
        m_arity_id = kb::kb()->search_arity_id(m_literal.get_arity());
}


bool unifier_t::operator==(const unifier_t &x) const
{
    const hash_map<term_t, term_t> &map1(m_mapping), &map2(x.m_mapping);
    if (map1.size() != map2.size()) return false;

    for (auto it = map1.begin(); it != map1.end(); ++it)
    {
        auto found = map2.find(it->first);
        if (found == map2.end())
            return false;
        if (found->second != it->second)
            return false;
    }

    return true;
}


void unifier_t::operator()(literal_t *p_out_lit) const
{
    for (size_t i = 0; i < p_out_lit->terms.size(); i++)
    {
        term_t &term = p_out_lit->terms[i];
        auto found = m_mapping.find(term);

        if (found != m_mapping.end())
            term = found->second;
    }
}


bool unifier_t::do_contain(const unifier_t &x) const
{
    const hash_map<term_t, term_t> &map1(m_mapping), &map2(x.m_mapping);
    if (map1.size() < map2.size()) return false;

    for (auto it = map2.begin(); it != map2.end(); ++it)
    {
        auto found = map1.find(it->first);
        if (found == map1.end())
            return false;
        if (found->second != it->second)
            return false;
    }

    return true;
}


std::string unifier_t::to_string() const
{
    std::string exp;
    for (auto sub = m_substitutions.begin(); sub != m_substitutions.end(); ++sub)
    {
        if (sub->terms.at(0) != sub->terms.at(1))
        {
            if (not exp.empty()) exp += ", ";
            exp += sub->terms.at(0).string() + "/" + sub->terms.at(1).string();
        }
    }
    return "{" + exp + "}";
}


bool chain_candidate_t::operator>(const chain_candidate_t &x) const
{
    if (axiom_id != x.axiom_id) return (axiom_id > x.axiom_id);
    if (is_forward != x.is_forward) return is_forward;
    if (nodes.size() != x.nodes.size()) return (nodes.size() > x.nodes.size());

    std::vector<node_idx_t>::const_iterator
        it1(nodes.begin()), it2(x.nodes.begin());

    for (; it1 != nodes.end(); ++it1, ++it2)
    if ((*it1) != (*it2))
        return ((*it1) > (*it2));

    return false;
}


bool chain_candidate_t::operator<(const chain_candidate_t &x) const
{
    if (axiom_id != x.axiom_id) return (axiom_id < x.axiom_id);
    if (is_forward != x.is_forward) return !is_forward;
    if (nodes.size() != x.nodes.size()) return (nodes.size() < x.nodes.size());

    std::vector<node_idx_t>::const_iterator
        it1(nodes.begin()), it2(x.nodes.begin());

    for (; it1 != nodes.end(); ++it1, ++it2)
    if ((*it1) != (*it2))
        return ((*it1) < (*it2));

    return false;
}


bool chain_candidate_t::operator==(const chain_candidate_t &x) const
{
    if (axiom_id != x.axiom_id) return false;
    if (is_forward != x.is_forward) return false;
    if (nodes.size() != x.nodes.size()) return false;

    std::vector<node_idx_t>::const_iterator
        it1(nodes.begin()), it2(x.nodes.begin());

    for (; it1 != nodes.end(); ++it1, ++it2)
    if ((*it1) != (*it2))
        return false;

    return true;
}


bool chain_candidate_t::operator!=(const chain_candidate_t &x) const
{
    return not operator==(x);
}


void proof_graph_t::unifiable_variable_clusters_set_t::add(
    term_t t1, term_t t2 )
{
    m_variables.insert(t1);
    m_variables.insert(t2);
    
    hash_map<term_t, index_t>::iterator iter_c1 = m_map_v2c.find(t1);
    hash_map<term_t, index_t>::iterator iter_c2 = m_map_v2c.find(t2);
    bool has_found_t1(iter_c1 != m_map_v2c.end());
    bool has_found_t2(iter_c2 != m_map_v2c.end());
    
    if (not has_found_t1  and not has_found_t2)
    {
        m_idx_new_cluster++;
        m_clusters[m_idx_new_cluster].insert(t1);
        m_clusters[m_idx_new_cluster].insert(t2);
        m_map_v2c[t1] = m_idx_new_cluster;
        m_map_v2c[t2] = m_idx_new_cluster;
    }
    else if (has_found_t1 and has_found_t2)
    {
        // MERGE CLUSTERS OF t1 AND t2
        if (iter_c1->second != iter_c2->second)
        {
            hash_set<term_t>& c1 = m_clusters[iter_c1->second];
            hash_set<term_t>& c2 = m_clusters[iter_c2->second];

            for (auto it = c2.begin(); it != c2.end(); ++it)
                m_map_v2c[*it] = iter_c1->second;

            c1.insert(c2.begin(), c2.end());
            c2.clear();
        }
    }
    else if (has_found_t1 and not has_found_t2)
    {
        m_clusters[iter_c1->second].insert(t2);
        m_map_v2c[t2] = iter_c1->second;
    }
    else if (not has_found_t1 and has_found_t2)
    {
        m_clusters[iter_c2->second].insert(t1);
        m_map_v2c[t1] = iter_c2->second;
    }
}


void proof_graph_t::unifiable_variable_clusters_set_t::merge(
    const unifiable_variable_clusters_set_t &vc)
{
    int padding(0);
    for (auto it = m_clusters.begin(); it != m_clusters.end(); ++it)
        padding = (padding > it->first) ? it->first : padding;

    m_variables.insert(vc.m_variables.begin(), vc.m_variables.end());

    for (auto it = vc.m_clusters.begin(); it != vc.m_clusters.end(); ++it)
        m_clusters[it->first + padding].insert(
        it->second.begin(), it->second.end());

    for (auto it = vc.m_map_v2c.begin(); it != vc.m_map_v2c.end(); ++it)
        m_map_v2c[it->first] = (it->second + padding);
}


void proof_graph_t::temporal_variables_t::clear()
{
    postponed_unifications.clear();
    considered_unifications.clear();
    coexistability_logs.clear();
    argument_set_ids.clear();
}


std::list< const hash_set<term_t>* >
    proof_graph_t::enumerate_variable_clusters() const
{
    std::list< const hash_set<term_t>* > out;
    const hash_map<index_t, hash_set<term_t> >
        &clusters = m_vc_unifiable.clusters();

    for (auto it = clusters.begin(); it != clusters.end(); ++it)
        out.push_back(&it->second);

    return out;
}


hash_set<edge_idx_t> proof_graph_t::enumerate_dependent_edges(node_idx_t idx) const
{
    hash_set<edge_idx_t> out;
    enumerate_dependent_edges(idx, &out);
    return out;
}


void proof_graph_t::enumerate_dependent_edges(
    node_idx_t idx, hash_set<edge_idx_t> *out) const
{
    if (not node(idx).literal().is_equality())
    {
        hypernode_idx_t m = node(idx).master_hypernode();
        if (m < 0) return;

        edge_idx_t e = find_parental_edge(m);
        out->insert(e);

        auto _nodes = hypernode(edge(e).tail());
        for (auto it = _nodes.begin(); it != _nodes.end(); ++it)
            enumerate_dependent_edges(*it, out);
    }
}


void proof_graph_t::enumerate_dependent_nodes(
    node_idx_t idx, hash_set<node_idx_t> *out) const
{
    if (not node(idx).literal().is_equality())
    {
        hypernode_idx_t m = node(idx).master_hypernode();
        if (m < 0) return;

        edge_idx_t e = find_parental_edge(m);
        auto _nodes = hypernode(edge(e).tail());

        for (auto it = _nodes.begin(); it != _nodes.end(); ++it)
        {
            out->insert(*it);
            enumerate_dependent_nodes(*it, out);
        }
    }
}


bool proof_graph_t::check_availability_of_chain(
    pg::edge_idx_t idx, hash_set<node_idx_t> *subs1, hash_set<node_idx_t> *subs2) const
{
#ifndef DISABLE_CANCELING
    auto found_subs = m_subs_of_conditions_for_chain.find(idx);
    if (found_subs != m_subs_of_conditions_for_chain.end())
    {
        const std::list<std::pair<term_t, term_t> > &subs = found_subs->second;
        for (auto it = subs.begin(); it != subs.end(); ++it)
        {
            node_idx_t n = find_sub_node(it->first, it->second);
            if (n >= 0) subs1->insert(n);
            else return false;
        }
    }

    auto found_neqs = m_neqs_of_conditions_for_chain.find(idx);
    if (found_neqs != m_neqs_of_conditions_for_chain.end())
    {
        const std::list<std::pair<term_t, term_t> > &neqs = found_neqs->second;
        for (auto it = neqs.begin(); it != neqs.end(); ++it)
        {
            node_idx_t n = find_sub_node(it->first, it->second);
            if (n >= 0) subs2->insert(n);
        }
    }
#endif

    return true;
}


bool proof_graph_t::_check_nodes_coexistability(
    node_idx_t n1, node_idx_t n2, const unifier_t *uni) const
{
    // USES THE LOG ONLY IF uni == NULL.
    if (uni == NULL)
    {
        const bool *log = m_temporal.coexistability_logs.find(n1, n2);
        if (log != NULL) return (*log);
    }
    
    hash_set<edge_idx_t>
        e1(enumerate_dependent_edges(n1)),
        e2(enumerate_dependent_edges(n2));
    if (e1.size() > e2.size()) std::swap(e1, e2);

    for (auto it = e1.begin(); it != e1.end(); ++it)
    {
        // A EDGE SHARED BY e1 AND e2 IS SKIPPED.
        if (e2.count(*it) > 0) continue;

        auto found = m_mutual_exclusive_edges.find(*it);
        if (found == m_mutual_exclusive_edges.end()) continue;

        auto muex_edges = found->second;
        if (has_intersection<hash_set<edge_idx_t>::const_iterator>(
            muex_edges.begin(), muex_edges.end(), e2.begin(), e2.end()))
        {
            if (uni == NULL)
                m_temporal.coexistability_logs.insert(n1, n2, false);
            return false;
        }
    }

    hash_set<node_idx_t> ns1, ns2;
    enumerate_dependent_nodes(n1, &ns1);
    enumerate_dependent_nodes(n2, &ns2);
    ns1.insert(n1);
    ns2.insert(n2);
    if (ns1.size() > ns2.size()) std::swap(ns1, ns2);

    for (auto it = ns1.begin(); it != ns1.end(); ++it)
    {
        // SHARED BY BOTH OF ns1 and ns2, IT WILL BE SKIPPED.
        if (ns2.count(*it) > 0) continue;

        for (auto it2 = ns2.begin(); it2 != ns2.end(); ++it2)
        {
            const unifier_t *uni2 =
                search_mutual_exclusion_of_node(*it, *it2);

            if (uni2 != NULL)
            {
                // n1 AND n2 ARE ALWAYS MUTUAL-EXCLUSIVE.
                if (uni2->empty())
                {
                    if (uni == NULL)
                        m_temporal.coexistability_logs.insert(n1, n2, false);
                    return false;
                }

                // IF uni IS GIVEN, CHECKS UNIFIABILITY BETWEEN n1 AND n2.
                // NAMELY, RETURNS FALSE IF THE UNIFICATION BETWEEN n1 AND n2
                // VIOLATES ANY MUTUAL-EXCLUSION.
                if (uni != NULL)
                if (uni->do_contain(*uni2)) return false;
            }
        }
    }

    if (uni == NULL)
        m_temporal.coexistability_logs.insert(n1, n2, true);
    return true;
}


std::string proof_graph_t::hypernode2str(hypernode_idx_t i) const
{
    if (i >= 0 and i < m_hypernodes.size())
    {
        const std::vector<node_idx_t>& tail = hypernode(i);
        return format("%d:{", i) + join(tail.begin(), tail.end(), "%d", ",") + "}";
    }
    else
        return "-1:{}";
}


std::string proof_graph_t::edge_to_string( edge_idx_t i ) const
{
    std::ostringstream str_edge;
    const edge_t &_edge = edge(i);

    if (_edge.tail() >= 0)
    {
        const std::vector<node_idx_t>& tail = hypernode(_edge.tail());
        for( index_t j=0; j<tail.size(); ++j )
        {
            node_idx_t idx = tail.at(j);
            str_edge << ( j > 0 ? " ^ " : "" ) << node(idx).to_string();
        }
    }
    else
        str_edge << "none";

    switch(_edge.type())
    {
    case EDGE_UNDERSPECIFIED:
        str_edge << " => UNDERSPECIFIED => "; break;
    case EDGE_HYPOTHESIZE:
        str_edge << format(" => BACKWARD(axiom=%d) => ", _edge.axiom_id());
        break;
    case EDGE_IMPLICATION:
        str_edge << format(" => FORWARD(axiom=%d) => ", _edge.axiom_id());
        break;
    case EDGE_UNIFICATION:
        str_edge << " => UNIFY => "; break;
    default:
        str_edge << format(" => USER-DEFINED(type=%d) => ", _edge.type());
        break;               
    }

    if (_edge.head() >= 0)
    {
        const std::vector<node_idx_t>& head = hypernode(_edge.head());
        for (index_t j = 0; j<head.size(); ++j)
        {
            node_idx_t idx = head.at(j);
            str_edge << (j > 0 ? " ^ " : "") << node(idx).to_string();
        }
    }
    else
        str_edge << "none";
    
    return str_edge.str();
}


void proof_graph_t::enumerate_nodes_softly_unifiable(
const arity_t &arity, hash_set<node_idx_t> *out) const
{
    const hash_set<node_idx_t> *ns1 = search_nodes_with_arity(arity);
    if (ns1 != NULL)
        out->insert(ns1->begin(), ns1->end());

    if (kb::kb()->do_target_on_category_table(arity))
    {
        float threshold = phillip()->param_float(
            "threshold_soft_unify", kb::knowledge_base_t::get_max_distance());

        for (auto p1 : m_maps.predicate_to_nodes)
        for (auto p2 : p1.second)
        if (p2.first == 1)
        {
            arity_t arity2 = literal_t::get_arity(p1.first, p2.first, false);
            if (arity2 != arity)
            {
                float cost = kb::kb()->get_soft_unifying_cost(arity, arity2);
                if (cost >= 0.0 and cost < threshold)
                    out->insert(p2.second.begin(), p2.second.end());
            }
        }
    }
}


hash_set<node_idx_t>
    proof_graph_t::enumerate_nodes_with_literal( const literal_t &lit ) const
{
    hash_set<node_idx_t> out;
    const hash_set<node_idx_t> *pa_list =
        search_nodes_with_predicate( lit.predicate, lit.terms.size() );
    
    if (pa_list == NULL) return out;

    for (auto it = pa_list->begin(); it != pa_list->end(); ++it)
    {
        if (m_nodes.at(*it).literal() == lit)
            out.insert(*it);
    }

    return out;
}


hash_set<edge_idx_t> proof_graph_t::enumerate_edges_with_node(node_idx_t idx) const
{
    hash_set<edge_idx_t> out;

    auto edges_head = search_edges_with_node_in_head(idx);
    if (edges_head != NULL)
        out.insert(edges_head->begin(), edges_head->end());

    auto edges_tail = search_edges_with_node_in_tail(idx);
    if (edges_tail != NULL)
        out.insert(edges_tail->begin(), edges_tail->end());

    return out;
}


edge_idx_t proof_graph_t::find_parental_edge(hypernode_idx_t idx) const
{
    const hash_set<edge_idx_t> *_edges = search_edges_with_hypernode(idx);

    if (_edges != NULL)
    {
        for (auto e = _edges->begin(); e != _edges->end(); ++e)
        {
            const edge_t &ed = edge(*e);
            if (ed.head() == idx) return *e;
        }
    }

    return -1;
}


void proof_graph_t::enumerate_parental_edges(hypernode_idx_t idx, hash_set<edge_idx_t> *out) const
{
    const hash_set<edge_idx_t> *_edges = search_edges_with_hypernode(idx);
    if( _edges == NULL ) return;

    for (auto it = _edges->begin(); it != _edges->end(); ++it)
    {
        if (edge(*it).head() == idx)
            out->insert(*it);
    }
}


void proof_graph_t::enumerate_children_edges(
    hypernode_idx_t idx, hash_set<edge_idx_t> *out) const
{
    const hash_set<edge_idx_t> *_edges = search_edges_with_hypernode(idx);
    if( _edges == NULL ) return;

    for (auto it = _edges->begin(); it != _edges->end(); ++it)
    {
        if (edge(*it).tail() == idx)
            out->insert(*it);
    }
}


void proof_graph_t::enumerate_children_hypernodes(
    hypernode_idx_t idx, hash_set<hypernode_idx_t> *out) const
{
    const hash_set<edge_idx_t> *_edges = search_edges_with_hypernode(idx);
    if (_edges == NULL) return;

    for (auto it = _edges->begin(); it != _edges->end(); ++it)
    {
        const edge_t &e = edge(*it);
        if (e.tail() == idx and e.head() >= 0)
            out->insert(e.head());
    }
}


void proof_graph_t::enumerate_descendant_nodes(
    node_idx_t idx, hash_set<node_idx_t> *out) const
{
    hash_set<hypernode_idx_t> checked;
    std::function<void(node_idx_t, hash_set<node_idx_t>*, hash_set<hypernode_idx_t>*)> f;

    f = [this, &f](node_idx_t idx, hash_set<node_idx_t> *out, hash_set<hypernode_idx_t> *checked)
    {
        const hash_set<hypernode_idx_t> *hns = this->search_hypernodes_with_node(idx);

        if (hns != NULL)
        for (auto hn = hns->begin(); hn != hns->end(); ++hn)
        {
            hash_set<hypernode_idx_t> children;
            this->enumerate_children_hypernodes(*hn, &children);

            for (auto c = children.begin(); c != children.end(); ++c)
            {
                if (checked->count(*c) > 0) continue;
                else checked->insert(*c);

                auto _hn = this->hypernode(*c);
                out->insert(_hn.begin(), _hn.end());
                for (auto n = _hn.begin(); n != _hn.end(); ++n)
                    f(*n, out, checked);
            }
        }
    };

    f(idx, out, &checked);
}


void proof_graph_t::enumerate_parental_hypernodes(
    hypernode_idx_t idx, hash_set<hypernode_idx_t> *out) const
{
    const hash_set<edge_idx_t> *_edges = search_edges_with_hypernode(idx);
    if (_edges == NULL) return;

    for (auto it = _edges->begin(); it != _edges->end(); ++it)
    {
        const edge_t &e = edge(*it);
        if (e.head() == idx)
            out->insert(e.tail());
    }
}


void proof_graph_t::enumerate_overlapping_hypernodes(
    hypernode_idx_t idx, hash_set<hypernode_idx_t> *out) const
{
    auto ns = hypernode(idx);
    for (auto n = ns.begin(); n != ns.end(); ++n)
    {
        auto hns = search_hypernodes_with_node(*n);
        if (hns != NULL)
            out->insert(hns->begin(), hns->end());
    }
    out->insert(idx);
}


hypernode_idx_t proof_graph_t::find_hypernode_with_ordered_nodes(
    const std::vector<node_idx_t> &indices ) const
{
    const hash_set<hypernode_idx_t> *hypernodes
        = search_hypernodes_with_node(indices.at(0));
    if (hypernodes != NULL)
    {
        for (auto hn = hypernodes->begin(); hn != hypernodes->end(); ++hn)
        if (indices == hypernode(*hn))
            return *hn;
    }
    return -1;
}


node_idx_t proof_graph_t::find_sub_node(term_t t1, term_t t2) const
{
    const node_idx_t *found = m_maps.terms_to_sub_node.find(t1, t2);
    return (found != NULL) ? *found : -1;
}


node_idx_t proof_graph_t::find_neg_sub_node(term_t t1, term_t t2) const
{
    const node_idx_t *found = m_maps.terms_to_negsub_node.find(t1, t2);
    return (found != NULL) ? *found : -1;
}


node_idx_t proof_graph_t::
    find_transitive_sub_node(node_idx_t i, node_idx_t j) const
{
    const node_t &n1(node(i)), &n2(node(j));
    term_t t1_1(n1.literal().terms.at(0)), t1_2(n1.literal().terms.at(1));
    term_t t2_1(n2.literal().terms.at(0)), t2_2(n2.literal().terms.at(1));

    if (t1_1 == t2_1) return find_sub_node(t1_2, t2_2);
    if (t1_1 == t2_2) return find_sub_node(t1_2, t2_1);
    if (t1_1 == t2_1) return find_sub_node(t1_2, t2_2);
    if (t1_1 == t2_1) return find_sub_node(t1_2, t2_2);
    return -1;
}


edge_idx_t proof_graph_t::find_unifying_edge(node_idx_t i, node_idx_t j) const
{
    hypernode_idx_t hn(-1);
    {
        // TIPS: UNIFYING HYPERNODE IS ASCENDING-ORDER
        if (i >= j) std::swap(i, j);
        std::vector<node_idx_t> _hn;
        _hn.push_back(i);
        _hn.push_back(j);
        hn = find_hypernode_with_ordered_nodes(_hn);
        if (hn < 0) return -1;
    }

    const hash_set<edge_idx_t> *es = search_edges_with_hypernode(hn);
    for (auto it = es->begin(); it != es->end(); ++it)
    {
        const edge_t &e = edge(*it);
        if (e.type() == EDGE_UNIFICATION and e.tail() == hn)
            return (*it);
    }

    return -1;
}


bool proof_graph_t::axiom_has_applied(
    hypernode_idx_t hn, const lf::axiom_t &ax, bool is_backward ) const
{
    const hash_map< axiom_id_t, hash_set<hypernode_idx_t> >
        &map = is_backward ?
        m_maps.axiom_to_hypernodes_backward :
        m_maps.axiom_to_hypernodes_forward;

    auto it = map.find(ax.id);
    return ( it != map.end() ) ? (it->second.count(hn) > 0) : false;
}


void proof_graph_t::print(std::ostream *os) const
{
    (*os)
        << "<latent-hypotheses-set name=\"" << name()
        << "\" time=\"" << phillip()->get_time_for_lhs()
        << "\" timeout=\"" << (is_timeout() ? "yes" : "no");

    for (auto it = m_attributes.begin(); it != m_attributes.end(); ++it)
        (*os) << "\" " << it->first << "=\"" << it->second;
    
    (*os) << "\">" << std::endl;

    print_nodes(os);
    print_axioms(os);
    print_edges(os);
    print_subs(os);
    print_mutual_exclusive_nodes(os);
    print_mutual_exclusive_edges(os);

    (*os) << "</latent-hypotheses-set>" << std::endl;
}


void proof_graph_t::print_nodes(std::ostream *os) const
{
    (*os) << "<nodes num=\"" << m_nodes.size() << "\">" << std::endl;

    for (auto i = 0; i < m_nodes.size(); ++i)
    {
        (*os)
            << "<node "
            << "index=\"" << i
            << "\" depth=\"" << node(i).depth()
            << "\" master=\"" << hypernode2str(node(i).master_hypernode())
            << "\">" << node(i).literal().to_string()
            << "</node>"
            << std::endl;
    }

    (*os) << "</nodes>" << std::endl;
}


void proof_graph_t::print_axioms(std::ostream *os) const
{
    hash_set<axiom_id_t> set_axioms;
    std::list<axiom_id_t> list_axioms;

    for (auto it = m_edges.begin(); it != m_edges.end(); ++it)
    if (it->axiom_id() >= 0)
        set_axioms.insert(it->axiom_id());

    list_axioms.assign(set_axioms.begin(), set_axioms.end());
    list_axioms.sort();

    (*os) << "<axioms num=\"" << list_axioms.size() << "\">" << std::endl;
    for (auto ax = list_axioms.begin(); ax != list_axioms.end(); ++ax)
    {
        lf::axiom_t axiom = kb::knowledge_base_t::instance()->get_axiom(*ax);
        (*os)
            << "<axiom "
            << "id=\"" << axiom.id
            << "\" name=\"" << axiom.name
            << "\">" << axiom.func.to_string()
            << "</axiom>" << std::endl;
    }
    (*os) << "</axioms>" << std::endl;
}


void proof_graph_t::print_edges(std::ostream *os) const
{
    (*os) << "<edges num=\"" << m_edges.size() << "\">" << std::endl;
    for (auto i = 0; i < m_edges.size(); ++i)
    {
        const edge_t e = edge(i);
        std::string type;
        
        if (e.type() < EDGE_USER_DEFINED)
        {
            switch (e.type())
            {
            case EDGE_UNDERSPECIFIED: type = "underspecified"; break;
            case EDGE_HYPOTHESIZE: type = "abductive"; break;
            case EDGE_IMPLICATION: type = "deductive"; break;
            case EDGE_UNIFICATION: type = "unification"; break;
            default: type = "unknown";
            }
        }
        else type = format("user-defined(%d)", e.type());

        std::string gaps = join_functional(
            get_gaps_on_edge(i),
            [](const std::pair<arity_t, arity_t> &p){return p.first + ":" + p.second; }, ",");

        (*os) << "<edge id=\"" << i << "\" type=\"" << type
              << "\" tail=\"" << hypernode2str(e.tail())
              << "\" head=\"" << hypernode2str(e.head())
              << "\" axiom=\"" << e.axiom_id()
              << "\" gap=\"" << gaps;

        auto conds = m_subs_of_conditions_for_chain.find(i);
        if (conds != m_subs_of_conditions_for_chain.end())
        {
            (*os) << "\" conds=\"";
            for (auto p = conds->second.begin(); p != conds->second.end(); ++p)
            {
                if (p != conds->second.begin())
                    (*os) << ", ";
                (*os) << "(= "
                      << p->first.string() << " "
                      << p->second.string() << ")";
            }
        }

        (*os) << "\">" << edge_to_string(i)
              << "</edge>" << std::endl;
    }
    (*os) << "</edges>" << std::endl;
}


void proof_graph_t::print_subs(std::ostream *os) const
{
    auto subs = m_vc_unifiable.clusters();
    (*os) << "<substitutions>" << std::endl;

    for (auto it = subs.begin(); it != subs.end(); ++it)
    {
        (*os) << "<cluster id=\"" << it->first << "\">" << std::endl;
        for (auto t = it->second.begin(); t != it->second.end(); ++t)
            (*os) << "<term>" << t->string() << "</term>" << std::endl;
        (*os) << "</cluster>" << std::endl;
    }

    (*os) << "</substitutions>" << std::endl;
}


void proof_graph_t::print_mutual_exclusive_nodes(std::ostream *os) const
{
    const hash_map<node_idx_t, hash_map<node_idx_t, unifier_t> >
        &muexs = m_mutual_exclusive_nodes;
    int num(0);

    for (auto it = muexs.begin(); it != muexs.end(); ++it)
        num += it->second.size();

    (*os) << "<mutual_exclusive_nodes num=\""
          << num << "\">" << std::endl;

    for (auto it1 = muexs.begin(); it1 != muexs.end(); ++it1)
    for (auto it2 = it1->second.begin(); it2 != it1->second.end(); ++it2)
    {
        const node_t &n1 = node(it1->first);
        const node_t &n2 = node(it2->first);

        (*os)
            << "<xor node1=\"" << n1.index()
            << "\" node2=\"" << n2.index()
            << "\" subs=\"" << it2->second.to_string() << "\">"
            << n1.literal().to_string() << " _|_ "
            << n2.literal().to_string() << "</xor>" << std::endl;
    }

    (*os) << "</mutual_exclusive_nodes>" << std::endl;
}


void proof_graph_t::print_mutual_exclusive_edges(std::ostream *os) const
{
    const hash_map<edge_idx_t, hash_set<edge_idx_t> >
        &muexs = m_mutual_exclusive_edges;
    int num(0);

    for (auto it = muexs.begin(); it != muexs.end(); ++it)
        num += it->second.size();

    (*os) << "<mutual_exclusive_edges num=\"" << num << "\">" << std::endl;

    for (auto it1 = muexs.begin(); it1 != muexs.end(); ++it1)
    for (auto it2 = it1->second.begin(); it2 != it1->second.end(); ++it2)
        (*os) << "<xor edge1=\"" << it1->first
              << "\" edge2=\"" << (*it2) << "\"></xor>" << std::endl;

    (*os) << "</mutual_exclusive_edges>" << std::endl;
}


node_idx_t proof_graph_t::add_node(
    const literal_t &lit, node_type_e type, int depth,
    const hash_set<node_idx_t> &parents)
{
    node_t add(this, lit, type, m_nodes.size(), depth, parents);
    int n = static_cast<int>(lit.terms.size());
    node_idx_t out = m_nodes.size();
    
    m_nodes.push_back(add);
    m_maps.predicate_to_nodes[lit.predicate][n].insert(out);
    m_maps.depth_to_nodes[depth].insert(out);
    
    if(lit.is_equality())
    {
        term_t t1(lit.terms[0]), t2(lit.terms[1]);
        if (lit.truth)
            m_maps.terms_to_sub_node.insert(t1, t2, out);
        else
            m_maps.terms_to_negsub_node.insert(t1, t2, out);
    }
    else
    {
        const kb::knowledge_base_t *base = kb::knowledge_base_t::instance();
        std::string arity = lit.get_arity();

        for (int i = 0; i < lit.terms.size(); ++i)
        {
            unsigned id = base->search_argument_set_id(arity, i);
            if (id != kb::INVALID_ARGUMENT_SET_ID)
                m_temporal.argument_set_ids[std::make_pair(out, i)] = id;
        }
        if (add.arity_id() != kb::INVALID_ARITY_ID)
            m_maps.arity_to_nodes[add.arity_id()].insert(out);
    }
    
    for (unsigned i = 0; i < lit.terms.size(); i++)
    {
        const term_t& t = lit.terms.at(i);
        m_maps.term_to_nodes[t].insert(out);
    }

    return out;
}


edge_idx_t proof_graph_t::add_edge(const edge_t &edge)
{
    edge_idx_t idx = m_edges.size();

    m_maps.hypernode_to_edge[edge.head()].insert(idx);
    m_maps.hypernode_to_edge[edge.tail()].insert(idx);

    if (edge.head() >= 0)
    for (auto n_idx : hypernode(edge.head()))
        m_maps.head_node_to_edges[n_idx].insert(idx);
    
    for (auto n_idx : hypernode(edge.tail()))
        m_maps.tail_node_to_edges[n_idx].insert(idx);

    m_edges.push_back(edge);
    return idx;
}


hypernode_idx_t proof_graph_t::chain(
    const std::vector<node_idx_t> &from, const lf::axiom_t &axiom, bool is_backward)
{
    /* This is a sub-routine of chain.
       @param lits  Literals whom nodes hypothesized by this chain have.
       @param sub   Map from terms in axiom to terms in proof-graph.
       @param conds Terms pair which must be unified for this chain.
       @return If this chaining is invalid, returns false. */
    auto get_substitutions = [this](
        const std::vector<node_idx_t> &from,
        const lf::axiom_t &axiom, bool is_backward,
        std::vector<literal_t> *lits, hash_map<term_t, term_t> *subs,
        std::set<std::pair<term_t, term_t> > *conds) -> bool
    {
        auto generate_subs = [](
            term_t t_ax, term_t t_hy, hash_map<term_t, term_t> *subs,
            std::set<std::pair<term_t, term_t> > *conds) -> bool
        {
            auto find1 = subs->find(t_ax);

            if (t_ax.is_constant())
            {
                if (t_ax != t_hy)
                {
#ifndef DISABLE_CANCELING
                    if (t_hy.is_constant())
                        return false;
                    else
#endif
                        conds->insert(make_sorted_pair(t_ax, t_hy));
                }
            }
            else
            {
                if (find1 == subs->end())
                    (*subs)[t_ax] = t_hy;
                else if (t_hy != find1->second)
                {
                    if (t_ax.is_hard_term())
                        return false;
                    else
                        conds->insert(make_sorted_pair(t_hy, find1->second));
                }
            }

            return true;
        };

        auto substitute_term =
            [](const term_t &target, hash_map<term_t, term_t> *subs) -> term_t
        {
            if (target.is_constant())
                return target;

            auto find = subs->find(target);
            if (find != subs->end())
                return find->second;

            const std::string &str = target.string();
            int idx1(str.find('.')), idx2(str.find('/'));

            if (idx1 != std::string::npos or idx2 != std::string::npos)
            {
                auto npos(std::string::npos);
                int idx = (idx1 != npos and(idx2 == npos or idx1 > idx2)) ? idx1 : idx2;
                term_t t(str.substr(0, idx));
                auto find2 = subs->find(t);

                if (find2 != subs->end())
                    return term_t(find2->second.string() + str.substr(idx));
            }

            term_t u = term_t::get_unknown_hash();
            (*subs)[target] = u;
            return u;
        };

        const lf::logical_function_t &lhs = axiom.func.branch(0);
        const lf::logical_function_t &rhs = axiom.func.branch(1);
        std::vector<const literal_t*>
            ax_to(is_backward ? axiom.func.get_lhs() : axiom.func.get_rhs()),
            ax_from(is_backward ? axiom.func.get_rhs() : axiom.func.get_lhs());
        int n_eq(0);

        /* CREATE MAP OF TERMS */
        for (size_t i = 0; i < ax_from.size(); ++i)
        {
            if (ax_from.at(i)->is_equality())
            {
                ++n_eq;
                continue;
            }

            const literal_t &li_ax = *(ax_from.at(i));
            const literal_t &li_hy = node(from.at(i - n_eq)).literal();

            for (size_t j = 0; j<li_ax.terms.size(); ++j)
            {
                const term_t &t_ax(li_ax.terms.at(j)), &t_hy(li_hy.terms.at(j));
                if (not generate_subs(t_ax, t_hy, subs, conds))
                    return false;

                const std::string &s_ax(t_ax.string()), &s_hy(t_hy.string());
                size_t idx1 = s_ax.rfind('.');
                size_t idx2 = s_hy.rfind('/');

                if (idx1 != std::string::npos)
                {
                    std::string sub, suf(s_ax.substr(idx1 + 1));

                    if (idx2 != std::string::npos)
                    if (suf == s_hy.substr(idx2 + 1))
                        sub = (s_hy.substr(0, idx2));

                    if (sub.empty())
                        sub = (s_hy + "/" + suf);

                    term_t _t(s_ax.substr(0, idx1));
                    if (not generate_subs(_t, term_t(sub), subs, conds))
                        return false;
                }
            }
        }

        /* ADD CONDITIONS BY TRANSITIVITY */
        while (true)
        {
            size_t n(conds->size());

            for (auto it1 = conds->begin(); it1 != conds->end(); ++it1)
            for (auto it2 = it1; it2 != conds->end(); ++it2)
            if (it1 != it2)
            {
                if (it1->first == it2->first)
                    conds->insert(make_sorted_pair(it1->second, it2->second));
                else if (it1->first == it2->second)
                    conds->insert(make_sorted_pair(it1->second, it2->first));
                else if (it1->second == it2->first)
                    conds->insert(make_sorted_pair(it1->first, it2->second));
                else if (it1->second == it2->second)
                    conds->insert(make_sorted_pair(it1->first, it2->first));
            }

            if (conds->size() == n) break;
        }

        std::set<std::pair<term_t, term_t> > presup_neqs; // PRESUPOSSITION
        {
            hash_set<edge_idx_t> dep_edges;
            hash_set<node_idx_t> dep_nodes(from.begin(), from.end());

            for (auto idx = from.begin(); idx != from.end(); ++idx)
                enumerate_dependent_edges(*idx, &dep_edges);

            for (auto e = dep_edges.begin(); e != dep_edges.end(); ++e)
            {
                auto it_neqs = m_neqs_of_conditions_for_chain.find(*e);
                if (it_neqs != m_neqs_of_conditions_for_chain.end())
                    presup_neqs.insert(it_neqs->second.begin(), it_neqs->second.end());

                const std::vector<node_idx_t> &hn_tail = hypernode(edge(*e).tail());
                const std::vector<node_idx_t> &hn_head = hypernode(edge(*e).head());
                dep_nodes.insert(hn_head.begin(), hn_head.end());
                dep_nodes.insert(hn_tail.begin(), hn_tail.end());
            }

            auto obs = observation_indices();
            dep_nodes.insert(obs.begin(), obs.end());

#ifndef DISABLE_CANCELING
            // CHECK VALIDITY OF DEPENDANT EDGES
            for (auto it_e1 = dep_edges.begin(); it_e1 != dep_edges.end(); ++it_e1)
            for (auto it_e2 = it_e1; it_e2 != dep_edges.end(); ++it_e2)
            if (it_e1 != it_e2)
            {
                edge_idx_t e1(*it_e1), e2(*it_e2);
                if (e1 > e2) std::swap(e1, e2);

                auto found1 = m_mutual_exclusive_edges.find(e1);
                if (found1 != m_mutual_exclusive_edges.end())
                {
                    auto found2 = m_mutual_exclusive_edges.find(e2);
                    if (found2 != m_mutual_exclusive_edges.end())
                        return false;
                }
            }

            if (axiom.func.is_operator(lf::OPR_PARAPHRASE))
            for (auto idx : from)
            {
                auto _edges = search_edges_with_node_in_head(idx);
                if (_edges == NULL) continue;

                for (auto e_idx : (*_edges))
                if (edge(e_idx).axiom_id() == axiom.id)
                    return false;
            }
#endif

            // INSERT UN-EQUALITIES FROM MUTUAL-EXCLUSIONS AMONG EVIDENCES.
            for (auto it_n1 = dep_nodes.begin(); it_n1 != dep_nodes.end(); ++it_n1)
            for (auto it_n2 = it_n1; it_n2 != dep_nodes.end(); ++it_n2)
            if (it_n1 != it_n2)
            {
                node_idx_t n1(*it_n1), n2(*it_n2);
                const unifier_t *uni = m_mutual_exclusive_nodes.find(n1, n2);
                
                if (uni != NULL)
                {
                    if (not uni->empty())
                    {
                        for (auto p : uni->mapping())
                            presup_neqs.insert(make_sorted_pair(p.first, p.second));
                    }
#ifndef DISABLE_CANCELING
                    else
                        return false; // n1 AND n2 ARE ALWAYS MUTUAL EXCLUSIVE.
#endif
                }
            }
        }

#ifndef DISABLE_CANCELING
        /* CHECK VALIDITY OF CONDITIONS */
        for (auto it = conds->begin(); it != conds->end(); ++it)
        {
            if (it->first.is_constant() and it->second.is_constant())
                return false;
            if (presup_neqs.find(*it) != presup_neqs.end())
                return false;
        }
#endif

        /* SUBSTITUTE TERMS IN LITERALS */
        lits->assign(ax_to.size(), literal_t());
        for (size_t i = 0; i < ax_to.size(); ++i)
        {
            (*lits)[i] = *ax_to.at(i);
            for (size_t j = 0; j < (*lits)[i].terms.size(); ++j)
            {
                term_t &term = (*lits)[i].terms[j];
                term = substitute_term(term, subs);
            }
        }

        return true;
    };

#ifndef DISABLE_CANCELING
    /* If given mutual exclusions cannot be satisfied, return true. */
    auto check_validity_of_mutual_exclusiveness = [this](
        const std::vector<node_idx_t> &from, const std::set<std::pair<term_t, term_t> > &cond,
        const std::vector<std::list<std::tuple<node_idx_t, unifier_t> > > &muexs) -> bool
    {
        hash_set<edge_idx_t> dep_edges;
        hash_set<node_idx_t> evidences(from.begin(), from.end());
        std::set<std::pair<term_t, term_t> > eqs(cond);

        for (auto it_n = from.begin(); it_n != from.end(); ++it_n)
            enumerate_dependent_edges(*it_n, &dep_edges);

        // ENUMERATE EVIDENCES AND SUBS IN CONDITIONS OF EDGE.
        for (auto it_e = dep_edges.begin(); it_e != dep_edges.end(); ++it_e)
        {
            const std::vector<node_idx_t> &tail = hypernode(edge(*it_e).tail());
            evidences.insert(tail.begin(), tail.end());

            auto it_subs = m_subs_of_conditions_for_chain.find(*it_e);
            if (it_subs != m_subs_of_conditions_for_chain.end())
                eqs.insert(it_subs->second.begin(), it_subs->second.end());
        }

        // ENUMERATE SUBS IN EVIDENCES.
        for (auto it_n = evidences.begin(); it_n != evidences.end(); ++it_n)
        if (node(*it_n).is_equality_node())
        {
            const std::vector<term_t> &terms = node(*it_n).literal().terms;
            eqs.insert(std::make_pair(terms.at(0), terms.at(1)));
        }

        for (int i = 0; i < muexs.size(); ++i)
        for (auto it_muex = muexs.at(i).begin(); it_muex != muexs.at(i).end(); ++it_muex)
        {
            node_idx_t n = std::get<0>(*it_muex);
            const unifier_t &uni = std::get<1>(*it_muex);

            // IF THE MUTUAL-EXCLUSION IS CERTAINLY VIOLATED, RETURN FALSE.
            if (evidences.count(n))
            {
                if (uni.empty())
                    return false;
                else
                {
                    for (auto it = uni.mapping().begin(); it != uni.mapping().end(); ++it)
                    if (eqs.count(std::make_pair(it->first, it->second)) > 0)
                        return false;
                }
            }
        }

        return true;
    };
#endif

    auto print_for_debug = [this](
        const lf::axiom_t &axiom, bool is_backward,
        pg::hypernode_idx_t from, pg::hypernode_idx_t to)
    {
        const std::vector<pg::node_idx_t>
            &hn_from(hypernode(from)), &hn_to(hypernode(to));
        std::string
            header(is_backward ? "BackwardChain: " : "ForwardChain: "),
            str_from(join(hn_from.begin(), hn_from.end(), "%d", ",")),
            str_to(join(hn_to.begin(), hn_to.end(), "%d", ",")),
            arrow(is_backward ? "<=" : "=>");

        print_console_fmt("%s: %d:[%s] %s %s %s %d:[%s]",
            header.c_str(), from, str_from.c_str(), arrow.c_str(),
            axiom.name.c_str(), arrow.c_str(), to, str_to.c_str());
    };

    // TERMS IN PROOF-GRAPH TO BE UNIFIED EACH OTHER.
    std::set<std::pair<term_t, term_t> > conds;
    // LITERALS TO BE ADDED TO PROOF-GRAPH.
    std::vector<literal_t> added;
    // SUBSTITUTIONS FROM TERMS IN AXIOM TO TERMS IN PROOF-GRAPH.
    hash_map<term_t, term_t> subs;
    int depth(get_depth_of_deepest_node(from));

    assert(depth >= 0);
    if (not get_substitutions(from, axiom, is_backward, &added, &subs, &conds))
        return -1;

    /* CHECK VARIDITY OF CHAINING ABOUT MUTUAL-EXCLUSIVENESS */
    std::vector<std::list<std::tuple<node_idx_t, unifier_t> > > muexs(
        added.size(), std::list<std::tuple<node_idx_t, unifier_t> >());
    for (int i = 0; i < added.size(); ++i)
        get_mutual_exclusions(added.at(i), &(muexs[i]));

#ifndef DISABLE_CANCELING
    if (not check_validity_of_mutual_exclusiveness(from, conds, muexs))
        return -1;
#endif

    hypernode_idx_t idx_hn_from = add_hypernode(from);
    std::vector<node_idx_t> hn_to(added.size(), -1);

    hash_set<node_idx_t> parents(from.begin(), from.end());
    for (size_t i = 0; i < added.size(); ++i)
    {
        hash_set<node_idx_t> _ev;
        for (size_t j = 0; j < added.size(); ++j)
        if (j != i)
            _ev.insert(hn_to.at(j));

        int d = added[i].is_equality() ? -1 : depth + 1;
        hn_to[i] = add_node(added[i], NODE_HYPOTHESIS, d, parents);
    }
    hypernode_idx_t idx_hn_to = add_hypernode(hn_to);

    /* SET MASTER-HYPERNODE OF EACH NEW NODE */
    for (auto it = hn_to.begin(); it != hn_to.end(); ++it)
        m_nodes[*it].set_master_hypernode(idx_hn_to);

    /* ADD EDGE */
    edge_type_e type = (is_backward ? EDGE_HYPOTHESIZE : EDGE_IMPLICATION);
    edge_idx_t edge_idx = add_edge(edge_t(type, idx_hn_from, idx_hn_to, axiom.id));

    /* ADD CONDITIONS FOR CHAIN */
    {
        if (not conds.empty())
        {
            std::list< std::pair<term_t, term_t> > *cond_sub =
                &m_subs_of_conditions_for_chain[edge_idx];
            for (auto it = conds.begin(); it != conds.end(); ++it)
                cond_sub->push_back(std::make_pair(it->first, it->second));
        }

        // EQUALITY IN EVIDENCES IN THE AXIOM ARE CONSIDERED AS CONDITIONS.
        auto ax_from = (is_backward ? axiom.func.get_rhs() : axiom.func.get_lhs());
        std::list< std::pair<term_t, term_t> >
            *cond_neq = &m_neqs_of_conditions_for_chain[edge_idx],
            *cond_sub = &m_subs_of_conditions_for_chain[edge_idx];

        for (auto it = ax_from.begin(); it != ax_from.end(); ++it)
        if ((*it)->is_equality())
        {
            auto found1 = subs.find((*it)->terms.at(0));
            auto found2 = subs.find((*it)->terms.at(1));
            if (found1 != subs.end() and found2 != subs.end())
            {
                term_t t1(found1->second), t2(found2->second);
                if (t1 > t2) std::swap(t1, t2);
                cond_neq->push_back(std::make_pair(t1, t2));
            }
        }
    }

    if (phillip_main_t::verbose() >= VERBOSE_4)
        print_for_debug(axiom, is_backward, idx_hn_from, idx_hn_to);

    /* ADD AXIOM HISTORY */
    hash_map<axiom_id_t, hash_set<hypernode_idx_t> > &ax2hn = is_backward ?
        m_maps.axiom_to_hypernodes_backward : m_maps.axiom_to_hypernodes_forward;
    ax2hn[axiom.id].insert(idx_hn_from);

    /* GENERATE MUTUAL EXCLUSIONS BETWEEN CHAINS */
    bool flag(phillip()->flag("enable_node_based_mutual_exclusive_chain"));
    _generate_mutual_exclusion_for_edges(edge_idx, flag);

    /* GENERATE MUTUAL EXCLUSIONS & UNIFICATION ASSUMPTIONS */
    for (size_t i = 0; i < added.size(); ++i)
    {
        _generate_mutual_exclusions(hn_to[i], muexs[i]);
        _generate_unification_assumptions(hn_to[i]);
    }
    
    return idx_hn_to;
}


void proof_graph_t::get_mutual_exclusions(
    const literal_t &target,
    std::list<std::tuple<node_idx_t, unifier_t> > *muex) const
{
    _enumerate_mutual_exclusion_for_counter_nodes(target, muex);
    _enumerate_mutual_exclusion_for_inconsistent_nodes(target, muex);
    _enumerate_mutual_exclusion_for_argument_set(target, muex);
}


int proof_graph_t::get_depth_of_deepest_node(
    const std::vector<node_idx_t> &hn) const
{
    int out = -1;
    for (auto it = hn.begin(); it != hn.end(); ++it)
    {
        int depth = node(*it).depth();
        if (depth > out) out = depth;
    }
    return out;
}


std::list<std::pair<arity_t, arity_t> > proof_graph_t::get_gaps_on_edge(edge_idx_t idx) const
{
    const edge_t &e = edge(idx);
    std::list<std::pair<arity_t, arity_t> > out;

    if (e.axiom_id() < 0) return out;

    lf::axiom_t ax = kb::knowledge_base_t::instance()->get_axiom(e.axiom_id());
    std::vector<const lf::logical_function_t*> branches_tail;

    if (e.type() == EDGE_IMPLICATION)
        ax.func.branch(0).enumerate_literal_branches(&branches_tail);
    else if (e.type() == EDGE_HYPOTHESIZE)
        ax.func.branch(1).enumerate_literal_branches(&branches_tail);

    auto hypernode_tail = hypernode(e.tail());

    for (index_t i = 0; i < branches_tail.size(); ++i)
    {
        arity_t a1 = branches_tail.at(i)->literal().get_arity();
        arity_t a2 = node(hypernode_tail.at(i)).arity();

        if (a1 != a2)
            out.push_back(std::make_pair(a1, a2));
    }

    return out;
}


std::list<std::tuple<node_idx_t, node_idx_t, unifier_t> >
proof_graph_t::enumerate_mutual_exclusive_nodes() const
{
    const hash_map<node_idx_t, hash_map<node_idx_t, unifier_t> >
        &muexs = m_mutual_exclusive_nodes;
    std::list<std::tuple<node_idx_t, node_idx_t, unifier_t> > out;

    for (auto it1 = muexs.begin(); it1 != muexs.end(); ++it1)
    for (auto it2 = it1->second.begin(); it2 != it1->second.end(); ++it2)
        out.push_back(std::make_tuple(it1->first, it2->first, it2->second));

    return out;
}


std::list<hash_set<edge_idx_t> >
proof_graph_t::enumerate_mutual_exclusive_edges() const
{
    const hash_map<edge_idx_t, hash_set<edge_idx_t> >
        muexs(m_mutual_exclusive_edges);
    std::set<comparable_list<edge_idx_t> > buf;

    for (auto it = muexs.begin(); it != muexs.end(); ++it)
    {
        std::list<edge_idx_t> _list(it->second.begin(), it->second.end());
        _list.push_back(it->first);
        _list.sort();
        buf.insert(comparable_list<edge_idx_t>(_list));
    }

    std::list<hash_set<edge_idx_t> > out;
    for (auto it = buf.begin(); it != buf.end(); ++it)
        out.push_back(hash_set<edge_idx_t>(it->begin(), it->end()));

    return out;
}


void proof_graph_t::enumerate_queries_for_knowledge_base(
    node_idx_t pivot, std::list<kb::search_query_t> *out) const
{
    const kb::knowledge_base_t *base = kb::knowledge_base_t::instance();

    std::list<kb::search_query_t> queries;
    base->search_queries(node(pivot).arity_id(), &queries);

    for (auto q : queries)
    {
        std::vector<kb::arity_id_t> arities(std::get<0>(q).begin(), std::get<0>(q).end());
        hash_map<kb::arity_id_t, int> arity_count;
        hash_map<kb::arity_id_t, hash_set<node_idx_t> > a2ns;

        for (auto a : arities)
        {
            auto found = arity_count.find(a);
            if (found == arity_count.end()) arity_count[a] = 1;
            else ++(found->second);
        }

        for (auto p : arity_count)
        {
            if (p.first == node(pivot).arity_id() and p.second == 1)
                a2ns[p.first].insert(pivot);
            else
            {
                auto found = m_maps.arity_to_nodes.find(p.first);
                if (found != m_maps.arity_to_nodes.end())
                    a2ns.insert(std::make_pair(p.first, found->second));
                else break;
            }
        }

        // ADD NODES WHICH ARE SOFT-UNIFIABLE TO a2ns
        for (auto i : std::get<2>(q))
        {
            kb::arity_id_t a = arities.at(i);
            hash_set<node_idx_t> ns;

            enumerate_nodes_softly_unifiable(kb::kb()->search_arity(a), &ns);
            if (not ns.empty())
                a2ns[a].insert(ns.begin(), ns.end());
        }

        // q INCLUDING AN ARITY WHICH DOES NOT EXIST IN PROOF-GRAPH IS INVALID.
        if (a2ns.size() != arity_count.size()) continue;

        bool is_valid_query(true);

        // CHECK WHETHER ANY OF TERM PAIR SATISFIES HARD TERM CONSTRAINTS BY q.
        for (auto p : std::get<1>(q))
        {
            kb::term_pos_t &t1(p.first);
            kb::term_pos_t &t2(p.second);
            hash_set<term_t> terms;
            bool do_exist_term_pair(false);

            for (auto n : a2ns.at(t1.first))
                terms.insert(node(n).literal().terms.at(t1.second));

            for (auto n : a2ns.at(t2.first))
            {
                if (terms.count(node(n).literal().terms.at(t2.second)) > 0)
                {
                    do_exist_term_pair = true;
                    break;
                }
            }

            if (not do_exist_term_pair)
            {
                is_valid_query = false;
                break;
            }
        }

        if (is_valid_query)
            out->push_back(q);
    }
}


void proof_graph_t::_generate_mutual_exclusions(
    node_idx_t target,
    const std::list<std::tuple<node_idx_t, unifier_t> > &muexs)
{
    const literal_t &lit = node(target).literal();

    // ADD MUTUAL EXCLUSIONS FOR INCONSISTENCY
    for (auto it = muexs.begin(); it != muexs.end(); ++it)
    {
        node_idx_t idx2 = std::get<0>(*it);
        const unifier_t uni = std::get<1>(*it);

        IF_VERBOSE_FULL(
            "Inconsistent: " + node(target).to_string() + ", "
            + node(idx2).to_string() + uni.to_string());

        node_idx_t
            n1((target >= idx2) ? idx2 : target),
            n2((target >= idx2) ? target : idx2);
        m_mutual_exclusive_nodes[n1][n2] = uni;
    }
}


void proof_graph_t::_enumerate_mutual_exclusion_for_inconsistent_nodes(
    const literal_t &target1,
    std::list<std::tuple<node_idx_t, unifier_t> > *out) const
{
    if (target1.is_equality()) return;

    const kb::knowledge_base_t *kb = kb::knowledge_base_t::instance();
    std::string arity = target1.get_arity();
    kb::arity_id_t id1 = kb->search_arity_id(arity);

    for (auto p1 : m_maps.arity_to_nodes)
    {
        kb::arity_id_t id2 = p1.first;
        bool do_reverse = (id1 > id2);
        const std::list<std::pair<term_idx_t, term_idx_t> >* terms =
            do_reverse ?
            kb->search_inconsistent_terms(id2, id1) :
            kb->search_inconsistent_terms(id1, id2);
        if (terms == NULL) continue;

        for (auto idx : p1.second)
        {
            const literal_t &target2 = node(idx).literal();
            bool is_valid(true);
            unifier_t uni;

            for (auto t : (*terms))
            {
                const term_t &t1 = target1.terms.at(do_reverse ? t.second : t.first);
                const term_t &t2 = target2.terms.at(do_reverse ? t.first : t.second);
                if (t1 != t2)
                {
                    if (t1.is_constant() and t2.is_constant())
                    {
                        is_valid = false;
                        break;
                    }
                    else
                        uni.add(t1, t2);
                }
            }

            if (is_valid)
                out->push_back(std::make_tuple(idx, uni));
        }
    }
}


void proof_graph_t::_generate_unification_assumptions(node_idx_t target)
{
    if (node(target).literal().is_equality())
        return;

    /* Returns nodes which is unifiable with target. */
    auto enumerate_unifiable_nodes = [this](node_idx_t target) -> std::list<node_idx_t>
    {
        const literal_t &lit = node(target).literal();
        hash_set<node_idx_t> candidates;
        std::list<node_idx_t> unifiables;
        unifier_t unifier;

        enumerate_nodes_softly_unifiable(lit.get_arity(), &candidates);

        for (auto n : candidates)
        {
            if (target == n) continue;

            node_idx_t n1 = target;
            node_idx_t n2 = n;
            if (n1 > n2) std::swap(n1, n2);

            // IGNORE THE PAIR WHICH HAS BEEN CONSIDERED ALREADY.
            if (_is_considered_unification(n1, n2)) continue;
            else m_temporal.considered_unifications.insert(n1, n2); // ADD TO LOG

            // IF ONE IS THE ANCESTOR OF ANOTHER, THE PAIR CANNOT UNIFY.
            if (node(n1).ancestors().count(n2) > 0 or
                node(n2).ancestors().count(n1) > 0)
                continue;

            bool unifiable = check_unifiability(
                node(n1).literal(), node(n2).literal(), false, &unifier);

            // FILTERING WITH CO-EXISTENCY OF UNIFIED NODES
#ifndef DISABLE_CANCELING
            if (unifiable)
                unifiable = _check_nodes_coexistability(n1, n2, &unifier);
#endif

            if (unifiable and can_unify_nodes(n1, n2))
                unifiables.push_back(n);
        }

        return unifiables;
    };

    std::list<node_idx_t> unifiables = enumerate_unifiable_nodes(target);
    const kb::unification_postponement_t* pp =
        kb::knowledge_base_t::instance()->find_unification_postponement(node(target).arity());

    /* UNIFY EACH UNIFIABLE NODE PAIR. */
    for (auto it = unifiables.begin(); it != unifiables.end(); ++it)
    {
        if (pp != NULL)
        if (pp->do_postpone(this, target, *it))
        {
            node_idx_t n1(target), n2(*it);
            m_temporal.postponed_unifications.insert(n1, n2);

            IF_VERBOSE_FULL(
                format("Postponed unification: node[%d] - node[%d]", n1, n2));
            continue;
        }

        _chain_for_unification(target, *it);
    }
}


void proof_graph_t::_chain_for_unification(node_idx_t i, node_idx_t j)
{
    auto add_nodes_of_transitive_unification = [this](term_t t)
    {
        const hash_set<term_t> *terms = m_vc_unifiable.find_cluster(t);
        assert(terms != NULL);

        for (auto it = terms->begin(); it != terms->end(); ++it)
        {
            if (t == (*it)) continue;
            if (t.is_constant() and it->is_constant()) continue;

            /* GENERATE TRANSITIVE UNIFICATION. */
            if (find_sub_node(t, *it) < 0)
            {
                std::pair<term_t, term_t> ts = make_sorted_pair(t, *it);
                literal_t sub("=", ts.first, ts.second);
                node_idx_t idx = add_node(sub, NODE_HYPOTHESIS, -1, hash_set<node_idx_t>());
                m_maps.terms_to_sub_node.insert(ts.first, ts.second, idx);

                std::list<std::tuple<node_idx_t, unifier_t> > muex;
                get_mutual_exclusions(sub, &muex);
                _generate_mutual_exclusions(idx, muex);
            }
        }
    };

    std::vector<node_idx_t> unified_nodes; // FROM
    std::vector<node_idx_t> unify_nodes; // TO
    unifier_t uni;
    
    if (i >= j) std::swap(i, j);
    unified_nodes.push_back(i);
    unified_nodes.push_back(j);

    if (not check_unifiability(node(i).literal(), node(j).literal(), false, &uni))
        return;


    /* CREATE UNIFICATION-NODES & UPDATE VARIABLES. */
    const std::set<literal_t> &subs = uni.substitutions();
    hash_set<node_idx_t> parents(unified_nodes.begin(), unified_nodes.end());

    for (auto sub = subs.begin(); sub != subs.end(); ++sub)
    {
        term_t t1(sub->terms[0]), t2(sub->terms[1]);
        if (t1 == t2) continue;

        node_idx_t sub_node_idx = find_sub_node(t1, t2);
        if (sub_node_idx < 0)
        {
            if (t1 > t2) std::swap(t1, t2);
            sub_node_idx = add_node(*sub, NODE_HYPOTHESIS, -1, parents);

            m_maps.terms_to_sub_node.insert(t1, t2, sub_node_idx);
            m_vc_unifiable.add(t1, t2);

            std::list<std::tuple<node_idx_t, unifier_t> > muex;
            get_mutual_exclusions(*sub, &muex);
            _generate_mutual_exclusions(sub_node_idx, muex);
            add_nodes_of_transitive_unification(t1);
            add_nodes_of_transitive_unification(t2);
        }

        unify_nodes.push_back(sub_node_idx);
    }

    /* ADD UNIFICATION EDGE. */
    hypernode_idx_t hn_unified = add_hypernode(unified_nodes);
    hypernode_idx_t hn_unify = add_hypernode(unify_nodes);
    edge_t edge(EDGE_UNIFICATION, hn_unified, hn_unify, -1);

    m_indices_of_unification_hypernodes.insert(hn_unify);
    add_edge(edge);

    /* SET MASTER-HYPERNODE */
    for (auto it = unify_nodes.begin(); it != unify_nodes.end(); ++it)
        m_nodes[*it].set_master_hypernode(hn_unify);
}


bool proof_graph_t::check_unifiability(
    const literal_t &p1, const literal_t &p2, bool do_ignore_truthment,
    unifier_t *out )
{
    if (out != NULL) out->clear();

    if (not do_ignore_truthment and p1.truth != p2.truth) return false;
    if (p1.terms.size() != p2.terms.size()) return false;
    // if (p1.predicate != p2.predicate) return false;

    for (int i = 0; i < p1.terms.size(); i++)
    {
        if (p1.terms[i] != p2.terms[i])
        {
            if (p1.terms[i].is_constant() and p2.terms[i].is_constant())
                return false;
            else if (out != NULL)
                out->add(p1.terms[i], p2.terms[i]);
        }
    }
    return true;
}


std::mutex g_mutex_hasher;


size_t proof_graph_t::get_hash_of_nodes(std::list<node_idx_t> nodes)
{
    static std::hash<std::string> hasher;
    std::lock_guard<std::mutex> lock(g_mutex_hasher);
    nodes.sort();
    return hasher(join(nodes.begin(), nodes.end(), "%d", ","));    
}


void proof_graph_t::post_process()
{
    IF_VERBOSE_3("Generating postponed unification assumptions...");
    {
        bool do_break(false);
        while (not do_break)
        {
            do_break = true;

            for (auto it1 = m_temporal.postponed_unifications.begin();
                it1 != m_temporal.postponed_unifications.end();)
            {
                for (auto it2 = it1->second.begin(); it2 != it1->second.end();)
                {
                    const node_idx_t n1(it1->first), n2(*it2);
                    const kb::unification_postponement_t *pp =
                        kb::knowledge_base_t::instance()
                        ->find_unification_postponement(node(n1).arity());
                    assert(pp != NULL);

                    if (not pp->do_postpone(this, n1, n2))
                    {
                        _chain_for_unification(n1, n2);
                        do_break = false;
                        it2 = it1->second.erase(it2);
                    }
                    else
                        ++it2;
                }

                if (it1->second.empty())
                    it1 = m_temporal.postponed_unifications.erase(it1);
                else
                    ++it1;
            }
        }
    }

    IF_VERBOSE_3("Generating mutual exclusions among transitive equalities...");
    {
        for (auto terms : m_vc_unifiable.clusters())
        {
            std::set<std::pair<term_t, term_t> > muex_terms;

            for (auto t1 : terms.second)
            if (t1.is_constant())
            {
                for (auto t2 : terms.second)
                if (t2.is_constant())
                    muex_terms.insert(make_sorted_pair(t1, t2));
            }

            for (auto ts : muex_terms)
            for (auto t : terms.second)
            if (t != ts.first and t != ts.second)
            {
                std::pair<node_idx_t, node_idx_t> ns = make_sorted_pair(
                    find_sub_node(ts.first, t), find_sub_node(ts.second, t));
                if (ns.first >= 0 and ns.second >= 0 and ns.first != ns.second)
                    m_mutual_exclusive_nodes[ns.first][ns.second] = unifier_t();
            }
        }
    }

    IF_VERBOSE_4("Cleaned logs.");
    m_temporal.clear();
}


void proof_graph_t::add_requirement(const lf::logical_function_t &req)
{
    requirement_t add;
    add.is_gold = req.find_parameter("gold");

    if (req.is_operator(lf::OPR_LITERAL))
    {
        const literal_t &lit(req.literal());
        requirement_t::element_t e{ lit, -1 };

        if (not lit.is_equality())
            e.index = add_node(lit, NODE_REQUIRED, -1, hash_set<node_idx_t>());

        add.conjunction.push_back(e);
    }
    else if (req.is_operator(lf::OPR_AND))
    {
        for (auto br : req.branches())
        {
            assert(br.is_operator(lf::OPR_LITERAL));

            const literal_t &lit(br.literal());
            requirement_t::element_t e{ lit, -1 };

            if (not lit.is_equality())
                e.index = add_node(lit, NODE_REQUIRED, -1, hash_set<node_idx_t>());
            add.conjunction.push_back(e);
        }
    }

    if (not add.conjunction.empty())
        m_requirements.push_back(add);
}


hypernode_idx_t proof_graph_t::add_hypernode(
    const std::vector<index_t> &indices )
{
    if (indices.empty()) return -1;

    hypernode_idx_t idx = find_hypernode_with_ordered_nodes(indices);

    if (idx < 0)
    {
        m_hypernodes.push_back(indices);
        idx = m_hypernodes.size() - 1;
        for( auto it=indices.begin(); it!=indices.end(); ++it )
            m_maps.node_to_hypernode[*it].insert(idx);

        size_t h = get_hash_of_nodes(
            std::list<node_idx_t>(indices.begin(), indices.end()));
        m_maps.unordered_nodes_to_hypernode[h].insert(idx);
    }
    
    return idx;
}


void proof_graph_t::_enumerate_mutual_exclusion_for_counter_nodes(
    const literal_t &target,
    std::list<std::tuple<node_idx_t, unifier_t> > *out) const
{
    const hash_set<node_idx_t>* indices =
        search_nodes_with_predicate(target.predicate, target.terms.size());

    if (indices == NULL) return;

    for (auto it = indices->begin(); it != indices->end(); ++it)
    {
        const literal_t &l2 = node(*it).literal();

        if (target.truth != l2.truth)
        {
            unifier_t uni;

            if (check_unifiability(target, l2, true, &uni))
                out->push_back(std::make_tuple(*it, uni));
        }
    }
}


void proof_graph_t::_enumerate_mutual_exclusion_for_argument_set(
    const literal_t &target,
    std::list<std::tuple<node_idx_t, unifier_t> > *out) const
{
    if (target.is_equality()) return;

    kb::knowledge_base_t *_kb = kb::knowledge_base_t::instance();
    std::string arity = target.get_arity();
    hash_map<pg::node_idx_t, std::set<std::pair<term_idx_t, term_idx_t> > > cands;

    for (term_idx_t i = 0; i < target.terms.size(); ++i)
    {
        kb::argument_set_id_t id = _kb->search_argument_set_id(arity, i);
        if (id != kb::INVALID_ARGUMENT_SET_ID)
        {
            const std::map<std::pair<pg::node_idx_t, term_idx_t>, kb::argument_set_id_t>
                &ids = m_temporal.argument_set_ids;

            for (auto it = ids.begin(); it != ids.end(); ++it)
            if (id != it->second)
            {
                node_idx_t n = it->first.first;
                term_idx_t j = it->first.second;
                cands[n].insert(std::make_pair(i, j));
            }
        }
    }

    for (auto it = cands.begin(); it != cands.end(); ++it)
    {
        unifier_t uni;
        const node_t &n = node(it->first);

        for (auto it_pair = it->second.begin(); it_pair != it->second.end(); ++it_pair)
        {
            term_t t1 = target.terms.at(it_pair->first);
            term_t t2 = n.literal().terms.at(it_pair->second);
            if (t1 != t2)
                uni.add(t1, t2);
        }

        out->push_back(std::make_tuple(it->first, uni));
    }
}


void proof_graph_t::_generate_mutual_exclusion_for_edges(
    edge_idx_t target, bool is_node_base)
{
    const edge_t &e = edge(target);
    std::list< std::list<edge_idx_t> > grouped_edges;

    auto enumerate_exclusive_chains_from_node =
        [this](node_idx_t from, std::list< std::list<edge_idx_t> > *out)
    {
        const kb::knowledge_base_t *kb = kb::knowledge_base_t::instance();
        const hash_set<hypernode_idx_t> *hns = this->search_hypernodes_with_node(from);
        if (hns == NULL) return;

        // ENUMERATE EDGES CONNECTED WITH GIVEN NODE
        std::list<edge_idx_t> targets;
        for (auto it = hns->begin(); it != hns->end(); ++it)
        {
            const hash_set<edge_idx_t> *_edges = this->search_edges_with_hypernode(*it);
            if (_edges == NULL) continue;

            for (auto it_e = _edges->begin(); it_e != _edges->end(); ++it_e)
            {
                const edge_t &e = edge(*it_e);
                if (e.tail() == (*it) and e.axiom_id() >= 0)
                    targets.push_back(*it_e);
            }
        }

        if (not targets.empty())
        {
            // CREATE MAP OF EXCLUSIVENESS
            std::set< comparable_list<edge_idx_t> > exclusions;
            for (auto it1 = targets.begin(); it1 != targets.end(); ++it1)
            {
                const edge_t &e1 = edge(*it1);
                hash_set<axiom_id_t> grp = kb->search_axiom_group(e1.axiom_id());
                if (grp.empty()) continue;

                comparable_list<edge_idx_t> exc;
                for (auto it2 = targets.begin(); it2 != it1; ++it2)
                {
                    const edge_t &e2 = edge(*it2);
                    if (grp.count(e2.axiom_id()) > 0)
                        exc.push_back(*it2);
                }
                if (not exc.empty())
                {
                    exc.push_back(*it1);
                    exc.sort();
                    exclusions.insert(exc);
                }
            }

            // ADD TO OUT
            if (not exclusions.empty())
                out->insert(out->end(), exclusions.begin(), exclusions.end());
        }
    };

    auto enumerate_exclusive_chains_from_hypernode =
        [this](hypernode_idx_t from, std::list< std::list<edge_idx_t> > *out)
    {
        const kb::knowledge_base_t *kb = kb::knowledge_base_t::instance();
        const hash_set<edge_idx_t> *edges = this->search_edges_with_hypernode(from);
        if (edges == NULL) return;

        std::set< comparable_list<edge_idx_t> > exclusions;

        // CREATE MAP OF EXCLUSIVENESS
        for (auto it1 = edges->begin(); it1 != edges->end(); ++it1)
        {
            const edge_t &e1 = edge(*it1);
            if (e1.tail() != from or e1.axiom_id() < 0) continue;

            hash_set<axiom_id_t> grp = kb->search_axiom_group(e1.axiom_id());
            if (grp.empty()) continue;

            comparable_list<edge_idx_t> exc;
            for (auto it2 = edges->begin(); it2 != it1; ++it2)
            {
                const edge_t &e2 = edge(*it2);
                if (e2.tail() == from and e2.axiom_id() >= 0)
                if (grp.count(e2.axiom_id()) > 0)
                    exc.push_back(*it2);
            }
            if (not exc.empty())
            {
                exc.push_back(*it1);
                exc.sort();
                exclusions.insert(exc);
            }
        }

        // ADD TO OUT
        if (not exclusions.empty())
            out->insert(out->end(), exclusions.begin(), exclusions.end());
    };

    if (is_node_base)
    {
        const std::vector<node_idx_t> &tail = hypernode(e.tail());
        for (auto it = tail.begin(); it != tail.end(); ++it)
            enumerate_exclusive_chains_from_node(*it, &grouped_edges);
    }
    else
        enumerate_exclusive_chains_from_hypernode(e.tail(), &grouped_edges);

    for (auto it = grouped_edges.begin(); it != grouped_edges.end(); ++it)
    {
        for (auto it1 = it->begin(); it1 != it->end(); ++it1)
        for (auto it2 = it->begin(); it2 != it1; ++it2)
        {
            m_mutual_exclusive_edges[*it1].insert(*it2);
            m_mutual_exclusive_edges[*it2].insert(*it1);
        }
    }
}


}

}

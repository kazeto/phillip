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
        static int new_cluster = 0;
        new_cluster++;
        m_clusters[new_cluster].insert(t1);
        m_clusters[new_cluster].insert(t2);
        m_map_v2c[t1] = new_cluster;
        m_map_v2c[t2] = new_cluster;
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
    hypernode_idx_t m = node(idx).master_hypernode();
    if (m < 0) return;

    edge_idx_t e = find_parental_edge(m);
    out->insert(e);

    auto _nodes = hypernode(edge(e).tail());
    for (auto it = _nodes.begin(); it != _nodes.end(); ++it)
        enumerate_dependent_edges(*it, out);
}


void proof_graph_t::enumerate_dependent_nodes(
    node_idx_t idx, hash_set<node_idx_t> *out) const
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


void proof_graph_t::_enumerate_exclusive_chains_from_node(
    node_idx_t from, std::list< std::list<edge_idx_t> > *out) const
{
    const kb::knowledge_base_t *kb = sys()->knowledge_base();
    const hash_set<hypernode_idx_t> *hns = search_hypernodes_with_node(from);
    if (hns == NULL) return;

    // ENUMERATE EDGES CONNECTED WITH GIVEN NODE
    std::list<edge_idx_t> targets;
    for (auto it = hns->begin(); it != hns->end(); ++it)
    {
        const hash_set<edge_idx_t> *_edges = search_edges_with_hypernode(*it);
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
}


void proof_graph_t::_enumerate_exclusive_chains_from_hypernode(
    hypernode_idx_t from, std::list< std::list<edge_idx_t> > *out) const
{
    const kb::knowledge_base_t *kb = sys()->knowledge_base();
    const hash_set<edge_idx_t> *edges = search_edges_with_hypernode(from);
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
}


bool proof_graph_t::check_availability_of_chain(
    pg::edge_idx_t idx, hash_set<node_idx_t> *out) const
{
    auto find = m_subs_of_conditions_for_chain.find(idx);

    if (find != m_subs_of_conditions_for_chain.end())
    {
        const std::list<std::pair<term_t, term_t> > &subs = find->second;
        for (auto it = subs.begin(); it != subs.end(); ++it)
        {
            node_idx_t n = find_sub_node(it->first, it->second);
            if (n >= 0) out->insert(n);
            else return false;
        }
    }

    return true;
}


bool proof_graph_t::_check_nodes_coexistency(
    node_idx_t n1, node_idx_t n2, const unifier_t *uni) const
{
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
            return false;
    }

    hash_set<node_idx_t> ns1, ns2;
    enumerate_dependent_nodes(n1, &ns1);
    enumerate_dependent_nodes(n2, &ns2);
    ns1.insert(n1);
    ns2.insert(n2);
    if (ns1.size() > ns2.size()) std::swap(ns1, ns2);

    for (auto it = ns1.begin(); it != ns1.end(); ++it)
    {
        // A NODE SHARED BY ns1 and ns2 IS SKIPPED.
        if (ns2.count(*it) > 0) continue;

        for (auto it2 = ns2.begin(); it2 != ns2.end(); ++it2)
        {
            const unifier_t *_uni =
                search_mutual_exclusion_of_node(*it, *it2);
            if (_uni != NULL)
            {
                if (_uni->empty()) return false;

                // IF uni IS GIVEN, CHECKS UNIFIABILITY BETWEEN n1 AND n2.
                // THIS METHOD RETURNS FALSE IF THE UNIFICATION BETWEEN n1 AND n2
                // VIOLATES ANY MUTUAL-EXCLUSION.
                if (uni != NULL)
                if (uni->do_contain(*_uni)) return false;
            }
        }
    }

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

    str_edge << i << "(" << _edge.tail() << "=>" << _edge.head() << "): ";

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


edge_idx_t proof_graph_t::find_parental_edge( hypernode_idx_t idx ) const
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


void proof_graph_t::enumerate_parental_edges(
    hypernode_idx_t idx, std::list<edge_idx_t> *out) const
{
    const hash_set<edge_idx_t> *_edges = search_edges_with_hypernode(idx);
    if( _edges == NULL ) return;

    for (auto it = _edges->begin(); it != _edges->end(); ++it)
    {
        if (edge(*it).head() == idx)
            out->push_back(*it);
    }
}


void proof_graph_t::enumerate_children_edges(
    hypernode_idx_t idx, std::list<edge_idx_t> *out) const
{
    const hash_set<edge_idx_t> *_edges = search_edges_with_hypernode(idx);
    if( _edges == NULL ) return;

    for (auto it = _edges->begin(); it != _edges->end(); ++it)
    {
        if (edge(*it).tail() == idx)
            out->push_back(*it);
    }
}


void proof_graph_t::enumerate_hypernodes_children(
    hypernode_idx_t idx, std::list<hypernode_idx_t> *out) const
{
    const hash_set<edge_idx_t> *_edges = search_edges_with_hypernode(idx);
    if (_edges == NULL) return;

    for (auto it = _edges->begin(); it != _edges->end(); ++it)
    {
        const edge_t &e = edge(*it);
        if (e.tail() == idx)
            out->push_back(e.head());
    }
}


void proof_graph_t::enumerate_hypernodes_parents(
    hypernode_idx_t idx, std::list<hypernode_idx_t> *out) const
{
    const hash_set<edge_idx_t> *_edges = search_edges_with_hypernode(idx);
    if (_edges == NULL) return;

    for (auto it = _edges->begin(); it != _edges->end(); ++it)
    {
        const edge_t &e = edge(*it);
        if (e.head() == idx)
            out->push_back(e.tail());
    }
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
    if (t1 > t2) std::swap(t1, t2);

    auto it = m_maps.terms_to_sub_node.find(t1);
    if (it == m_maps.terms_to_sub_node.end())
        return -1;

    auto it2 = it->second.find(t2);
    if (it2 == it->second.end())
        return -1;

    return it2->second;
}


node_idx_t proof_graph_t::find_neg_sub_node(term_t t1, term_t t2) const
{
    if (t1 > t2) std::swap(t1, t2);

    auto it = m_maps.terms_to_negsub_node.find(t1);
    if (it == m_maps.terms_to_negsub_node.end())
        return -1;

    auto it2 = it->second.find(t2);
    if (it2 == it->second.end())
        return -1;

    return it2->second;
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


void proof_graph_t::insert_transitive_sub_node(hash_set<node_idx_t> *target) const
{
    hash_set<node_idx_t> add;

    for (auto n1 = target->begin(); n1 != target->end(); ++n1)
    for (auto n2 = target->begin(); n2 != n1; ++n2)
    {
        node_idx_t idx = find_transitive_sub_node(*n1, *n2);
        if (idx >= 0) add.insert(idx);
    }

    while (not add.empty())
    {
        hash_set<node_idx_t> next;

        for (auto n1 = add.begin(); n1 != add.end(); ++n1)
        {
            if (target->count(*n1) > 0) continue;

            for (auto n2 = target->begin(); n2 != target->end(); ++n2)
            {
                node_idx_t idx = find_transitive_sub_node(*n1, *n2);
                if (idx >= 0) next.insert(idx);
            }

            target->insert(*n1);
        }

        add = next;
    }
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
        << "\" time=\"" << sys()->get_time_for_lhs()
        << "\" time-out=\"" << (is_timeout() ? "yes" : "no");

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
        lf::axiom_t axiom = sys()->knowledge_base()->get_axiom(*ax);
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

        (*os) << "<edge id=\"" << i << "\" type=\"" << type
              << "\" tail=\"" << hypernode2str(e.tail())
              << "\" head=\"" << hypernode2str(e.head())
              << "\" axiom=\"" << e.axiom_id();

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
    const hash_set<node_idx_t> &evidences)
{
    node_t add(lit, type, m_nodes.size(), depth, evidences);
    node_idx_t out = m_nodes.size();
    
    m_nodes.push_back(add);
    m_maps.predicate_to_nodes[lit.predicate][lit.terms.size()].insert(out);
    m_maps.depth_to_nodes[depth].insert(out);
    
    if(lit.predicate == "=")
    {
        term_t t1(lit.terms[0]), t2(lit.terms[1]);
        if (t1 > t2) std::swap(t1, t2);

        if (lit.truth)
            m_maps.terms_to_sub_node[t1][t2] = out;
        else
            m_maps.terms_to_negsub_node[t1][t2] = out;
    }
    
    for (unsigned i = 0; i < lit.terms.size(); i++)
    {
        const term_t& t = lit.terms.at(i);
        m_maps.term_to_nodes[t].insert(out);
    }

    return out;
}


hypernode_idx_t proof_graph_t::chain(
    const std::vector<node_idx_t> &from,
    const lf::axiom_t &axiom, bool is_backward)
{
    int depth = get_depth_of_deepest_node(from);
    assert(depth >= 0);

    std::vector<literal_t> literals_to;
    hash_map<term_t, term_t> subs;
    hash_map<term_t, hash_set<term_t> > conds;

    _get_substitutions_for_chain(
        from, axiom, is_backward, &literals_to, &subs, &conds);

    /* CHECK VARIDITY OF CHAINING ABOUT MUTUAL-EXCLUSIVENESS */
    std::vector<std::list<std::tuple<node_idx_t, unifier_t, axiom_id_t> > > muexs;
    if (not _check_mutual_exclusiveness_for_chain(from, literals_to, &muexs))
        return -1;

    hypernode_idx_t idx_hn_from = add_hypernode(from);

    /* ADD NEW NODES AND NEW HYPERNODE TO THIS */
    std::vector<node_idx_t> hypernode_to(literals_to.size(), -1);
    hash_set<node_idx_t> evidences = _enumerate_evidences_for_chain(from);
    for (size_t i = 0; i < literals_to.size(); ++i)
    {
        hypernode_to[i] =
            add_node(literals_to[i], NODE_HYPOTHESIS, depth + 1, evidences);
    }
    hypernode_idx_t idx_hn_to = add_hypernode(hypernode_to);

    /* SET MASTER-HYPERNODE OF EACH NEW NODE */
    for (auto it = hypernode_to.begin(); it != hypernode_to.end(); ++it)
        m_nodes[*it].set_master_hypernode(idx_hn_to);

    /* ADD EDGE */
    edge_type_e type = (is_backward ? EDGE_HYPOTHESIZE : EDGE_IMPLICATION);
    edge_idx_t edge_idx =
        add_edge(edge_t(type, idx_hn_from, idx_hn_to, axiom.id));

    /* ADD CONDITIONAL UNIFICATION FOR CHAIN */
    if (not conds.empty())
    {
        std::list< std::pair<term_t, term_t> > *cd =
            &m_subs_of_conditions_for_chain[edge_idx];

        for (auto it = conds.begin(); it != conds.end(); ++it)
        for (auto t = it->second.begin(); t != it->second.end(); ++t)
            cd->push_back(std::make_pair(it->first, *t));
    }

    /* ADD AXIOM HISTORY */
    hash_map<axiom_id_t, hash_set<hypernode_idx_t> > &ax2hn = is_backward ?
        m_maps.axiom_to_hypernodes_backward : m_maps.axiom_to_hypernodes_forward;
    ax2hn[axiom.id].insert(idx_hn_from);

    /* GENERATE MUTUAL EXCLUSIONS BETWEEN CHAINS */
    bool flag(sys()->flag("enable_node_based_mutual_exclusive_chain"));
    _generate_mutual_exclusion_for_edges(edge_idx, flag);

    /* GENERATE MUTUAL EXCLUSIONS & UNIFICATION ASSUMPTIONS */
    for (size_t i = 0; i < literals_to.size(); ++i)
    {
        _generate_mutual_exclusions(hypernode_to[i], &muexs[i]);
        _generate_unification_assumptions(hypernode_to[i]);
    }
    
    return idx_hn_to;
}


void proof_graph_t::_get_substitutions_for_chain(
    const std::vector<node_idx_t> &from,
    const lf::axiom_t &axiom, bool is_backward,
    std::vector<literal_t> *lits, hash_map<term_t, term_t> *subs,
    hash_map<term_t, hash_set<term_t> > *conds) const
{
    const lf::logical_function_t &lhs = axiom.func.branch(0);
    const lf::logical_function_t &rhs = axiom.func.branch(1);
    std::vector<const literal_t*>
        ax_to((is_backward ? lhs : rhs).get_all_literals()),
        ax_from((is_backward ? rhs : lhs).get_all_literals());
    assert(from.size() == ax_from.size());

    /* CREATE MAP OF TERMS */
    for (size_t i=0; i<from.size(); ++i)
    {
        const literal_t &li_ax = *(ax_from.at(i));
        const literal_t &li_hy = node(from.at(i)).literal();

        for (size_t j=0; j<li_ax.terms.size(); ++j)
        {
            const term_t &t_ax(li_ax.terms.at(j)), &t_hy(li_hy.terms.at(j));
            _get_substitutions_for_chain_sub(t_ax, t_hy, subs, conds);

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

                _get_substitutions_for_chain_sub(
                    term_t(s_ax.substr(0, idx1)), term_t(sub), subs, conds);
            }
        }
    }

    /* SUBSTITUTE TERMS IN LITERALS */
    lits->assign(ax_to.size(), literal_t());
    for (size_t i = 0; i < ax_to.size(); ++i)
    {
        (*lits)[i] = *ax_to.at(i);
        for (size_t j = 0; j < (*lits)[i].terms.size(); ++j)
        {
            term_t &term = (*lits)[i].terms[j];
            term = _substitute_term_for_chain(term, subs);
        }
    }
}


void proof_graph_t::_get_substitutions_for_chain_sub(
    term_t t_from, term_t t_to,
    hash_map<term_t, term_t> *subs,
    hash_map<term_t, hash_set<term_t> > *conds) const
{
    auto find1 = subs->find(t_from);

    if (find1 == subs->end())
        (*subs)[t_from] = t_to;
    else if (t_to != find1->second)
    {
        term_t t1(t_to), t2(find1->second);
        if (t1 > t2) std::swap(t1, t2);

        auto find2 = conds->find(t1);
        if (find2 == conds->end())
            (*conds)[t1].insert(t2);
        else
            find2->second.insert(t2);
    }
}


term_t proof_graph_t::_substitute_term_for_chain(
    const term_t &target, hash_map<term_t, term_t> *subs) const
{
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
}


bool proof_graph_t::_check_mutual_exclusiveness_for_chain(
    const std::vector<node_idx_t> &from,
    const std::vector<literal_t> &to,
    std::vector<std::list<
    std::tuple<node_idx_t, unifier_t, axiom_id_t> > > *muexs) const
{
    hash_set<node_idx_t> dep = _enumerate_evidences_for_chain(from);

    muexs->assign(
        to.size(), std::list<std::tuple<node_idx_t, unifier_t, axiom_id_t> >());

    for (int i = 0; i < to.size(); ++i)
    {
        // ENUMERATE MUTUAL EXCLUSIONS
        _enumerate_mutual_exclusion_for_counter_nodes(to.at(i), &(*muexs)[i]);
        _enumerate_mutual_exclusion_for_inconsistent_nodes(to.at(i), &(*muexs)[i]);
        if (muexs->at(i).empty()) continue;

        // CHECK VARIDITY
        auto _muex = muexs->at(i);
        for (auto it = _muex.begin(); it != _muex.end(); ++it)
        {
            node_idx_t n = std::get<0>(*it);
            const unifier_t &uni = std::get<1>(*it);

#ifndef DISABLE_CUTTING_LHS
            if (dep.count(n) > 0 and uni.empty())
                return false;
#endif
        }
    }

    return true;
}


hash_set<node_idx_t> proof_graph_t::_enumerate_evidences_for_chain(
    const std::vector<node_idx_t> &from) const
{
    hash_set<node_idx_t> out;
    for (auto it = from.begin(); it != from.end(); ++it)
    {
        auto ev = node(*it).evidences();
        out.insert(ev.begin(), ev.end());
    }
    return out;
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


hash_set<node_idx_t> proof_graph_t::enumerate_observations() const
{
    hash_set<node_idx_t> out;
    for (node_idx_t i = 0; i < nodes().size(); ++i)
    if (node(i).type() == NODE_OBSERVABLE)
        out.insert(i);
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


void proof_graph_t::_generate_mutual_exclusions(
    node_idx_t target,
    std::list<std::tuple<node_idx_t, unifier_t, axiom_id_t> > *muexs)
{
    const literal_t &lit = node(target).literal();
    bool do_enumerate = (muexs == NULL);

    if (do_enumerate)
    {
        muexs = new std::list<std::tuple<node_idx_t, unifier_t, axiom_id_t> >();
        _enumerate_mutual_exclusion_for_inconsistent_nodes(lit, muexs);
        _enumerate_mutual_exclusion_for_counter_nodes(lit, muexs);
    }

    // ADD MUTUAL EXCLUSIONS FOR INCONSISTENCY
    for (auto it = muexs->begin(); it != muexs->end(); ++it)
    {
        node_idx_t idx2 = std::get<0>(*it);
        const unifier_t uni = std::get<1>(*it);
        axiom_id_t axiom_id = std::get<2>(*it);

        IF_VERBOSE_FULL(
            "Inconsistent: " + node(target).to_string() + ", "
            + node(idx2).to_string() + uni.to_string());

        if (axiom_id >= 0)
        {
            m_maps.node_to_inconsistency[target].insert(axiom_id);
            m_maps.node_to_inconsistency[idx2].insert(axiom_id);
        }

        node_idx_t
            n1((target >= idx2) ? idx2 : target),
            n2((target >= idx2) ? target : idx2);
        m_mutual_exclusive_nodes[n1][n2] = uni;
    }

    if (do_enumerate) delete muexs;
}


void proof_graph_t::_enumerate_mutual_exclusion_for_inconsistent_nodes(
    const literal_t &target,
    std::list<std::tuple<node_idx_t, unifier_t, axiom_id_t> > *out) const
{
    const kb::knowledge_base_t *kb = sys()->knowledge_base();
    std::string arity = target.get_predicate_arity();
    std::list<axiom_id_t> axioms = kb->search_inconsistencies(arity);

    for (auto ax = axioms.begin(); ax != axioms.end(); ++ax)
    {
        lf::axiom_t axiom = kb->get_axiom(*ax);

        const literal_t &lit1 = axiom.func.branch(0).literal();
        const literal_t &lit2 = axiom.func.branch(1).literal();
        bool do_rev = (lit1.get_predicate_arity() != arity);

        const hash_set<node_idx_t> *idx_nodes = do_rev ?
            search_nodes_with_predicate(lit1.predicate, lit1.terms.size()) :
            search_nodes_with_predicate(lit2.predicate, lit2.terms.size());

        if (idx_nodes == NULL) continue;

        for (auto it = idx_nodes->begin(); it != idx_nodes->end(); ++it)
        {
            const literal_t &target2 = node(*it).literal();
            const literal_t &inc_1 = (do_rev ? lit2 : lit1);
            const literal_t &inc_2 = (do_rev ? lit1 : lit2);
            unifier_t uni;

            for (unsigned t1 = 0; t1 < target.terms.size(); t1++)
            for (unsigned t2 = 0; t2 < target2.terms.size(); t2++)
            {
                if (inc_1.terms.at(t1) == inc_2.terms.at(t2))
                {
                    const term_t &term1 = target.terms.at(t1);
                    const term_t &term2 = target2.terms.at(t2);
                    if (term1 != term2) uni.add(term1, term2);
                }
            }

            out->push_back(std::make_tuple(*it, uni, *ax));
        }
    }
}


void proof_graph_t::_generate_unification_assumptions(node_idx_t target)
{
    if (node(target).is_equality_node() or
        node(target).is_non_equality_node())
        return;

    std::list<node_idx_t> unifiables = _enumerate_unifiable_nodes(target);

    /* UNIFY EACH UNIFIABLE NODE PAIR. */
    for (auto it = unifiables.begin(); it != unifiables.end(); ++it)
        _chain_for_unification(target, *it);
}


std::list<node_idx_t>
    proof_graph_t::_enumerate_unifiable_nodes(node_idx_t target)
{
    const literal_t &lit = node(target).literal();
    auto candidates =
        search_nodes_with_predicate(lit.predicate, lit.terms.size());
    std::list<node_idx_t> unifiables;
    unifier_t unifier;

    for (auto it = candidates->begin(); it != candidates->end(); ++it)
    {
        if (target == (*it)) continue;

        node_idx_t n1 = target;
        node_idx_t n2 = (*it);
        if (n1 > n2) std::swap(n1, n2);

        // IGNORE THE PAIR WHICH HAS BEEN CONSIDERED ALREADY.
        if (_is_considered_unification(n1, n2)) continue;
        else m_logs.considered_unifications[n1].insert(n2); // ADD TO LOG

        // IF ONE IS THE ANCESTOR OF ANOTHER, THE PAIR CANNOT UNIFY.
        if (node(n1).evidences().count(n2) > 0 or
            node(n2).evidences().count(n1) > 0)
            continue;

        bool unifiable = check_unifiability(
            node(n1).literal(), node(n2).literal(), false, &unifier);

#ifndef DISABLE_CUTTING_LHS
        // FILTERING WITH CO-EXISTENCY OF UNIFIED NODES
        if (unifiable)
            unifiable = _check_nodes_coexistency(n1, n2, &unifier);
#endif

        if (unifiable and can_unify_nodes(n1, n2))
            unifiables.push_back(*it);
    }

    return unifiables;
}


void proof_graph_t::_chain_for_unification(node_idx_t i, node_idx_t j)
{
    std::vector<node_idx_t> unified_nodes; // FROM
    std::vector<node_idx_t> unify_nodes; // TO
    unifier_t uni;
    
    if (i >= j) std::swap(i, j);
    unified_nodes.push_back(i);
    unified_nodes.push_back(j);

    if (not check_unifiability(node(i).literal(), node(j).literal(), false, &uni))
        return;

    hash_set<node_idx_t> evidences =
        _enumerate_evidences_for_chain(unified_nodes);

    /* CREATE UNIFICATION-NODES & UPDATE VARIABLES. */
    const std::set<literal_t> &subs = uni.substitutions();
    for (auto sub = subs.begin(); sub != subs.end(); ++sub)
    {
        term_t t1(sub->terms[0]), t2(sub->terms[1]);
        if (t1 == t2) continue;

        node_idx_t sub_node_idx = find_sub_node(t1, t2);
        if (sub_node_idx < 0)
        {
            if (t1 > t2) std::swap(t1, t2);
            sub_node_idx = add_node(*sub, NODE_HYPOTHESIS, -1, evidences);

            m_maps.terms_to_sub_node[t1][t2] = sub_node_idx;
            m_vc_unifiable.add(t1, t2);
            _generate_mutual_exclusions(sub_node_idx);
            _add_nodes_of_transitive_unification(t1);
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


void proof_graph_t::_add_nodes_of_transitive_unification(term_t t)
{
    const hash_set<term_t> *terms = m_vc_unifiable.find_cluster(t);
    assert( terms != NULL );

    for( auto it = terms->begin(); it != terms->end(); ++it )
    {
        if( t == (*it) ) continue;
        if( t.is_constant() and it->is_constant() ) continue;

        /* GENERATE TRANSITIVE UNIFICATION. */
        if (find_sub_node(t, *it) < 0)
        {
            term_t t1(t), t2(*it);
            if (t1 > t2) std::swap(t1, t2);

            node_idx_t idx = add_node(
                literal_t("=", t1, t2), NODE_HYPOTHESIS,
                -1, hash_set<node_idx_t>());
            m_maps.terms_to_sub_node[t1][t2] = idx;
            _generate_mutual_exclusions(idx);
        }
    }
}


/** Return whether p1 and p2 can be unified or not.
 *  This method is from getMGU(-) in henry-n700.
 *  @param[out] out The unifier for unification of p1 and p2. */
bool proof_graph_t::check_unifiability(
    const literal_t &p1, const literal_t &p2, bool do_ignore_truthment,
    unifier_t *out )
{
    out->clear();

    if (not do_ignore_truthment and p1.truth != p2.truth) return false;
    if (p1.predicate != p2.predicate) return false;
    if (p1.terms.size() != p2.terms.size()) return false;

    for (int i = 0; i < p1.terms.size(); i++)
    {
        if (p1.terms[i] != p2.terms[i])
        {
            if (p1.terms[i].is_constant() and p2.terms[i].is_constant())
                return false;
            else
                out->add(p1.terms[i], p2.terms[i]);
        }
    }
    return true;
}


size_t proof_graph_t::get_hash_of_nodes(std::list<node_idx_t> nodes)
{
    static std::hash<std::string> hasher;
    nodes.sort();
    return hasher(join(nodes.begin(), nodes.end(), "%d", ","));    
}


void proof_graph_t::clean_logs()
{
    m_logs.considered_unifications.clear();
    m_logs.considered_exclusions.clear();
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
    std::list<std::tuple<node_idx_t, unifier_t, axiom_id_t> > *out) const
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
                out->push_back(std::make_tuple(*it, uni, -1));
        }
    }
}


void proof_graph_t::_generate_mutual_exclusion_for_edges(
    edge_idx_t target, bool is_node_base)
{
    const edge_t &e = edge(target);
    std::list< std::list<edge_idx_t> > grouped_edges;

    if (is_node_base)
    {
        const std::vector<node_idx_t> &tail = hypernode(e.tail());
        for (auto it = tail.begin(); it != tail.end(); ++it)
            _enumerate_exclusive_chains_from_node(*it, &grouped_edges);
    }
    else
        _enumerate_exclusive_chains_from_hypernode(e.tail(), &grouped_edges);

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

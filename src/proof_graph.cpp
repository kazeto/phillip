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


std::string unifier_t::to_string() const
{
    std::string exp;
    for( size_t i = 0; i < m_substitutions.size(); i++ )
    {
        const literal_t &sub = m_substitutions.at(i);

        if (sub.terms.at(0) == sub.terms.at(1)) continue;
        if (not exp.empty()) exp += ", ";

        exp += sub.terms.at(0).string() + "/" + sub.terms.at(1).string();
    }
    return "{" + exp + "}";
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


std::string proof_graph_t::unifiable_variable_clusters_set_t::to_string() const
{
    std::ostringstream ret;
    for (auto it_ec = m_clusters.begin(); it_ec != m_clusters.end(); ++it_ec)
    {
        const hash_set<term_t> &cluster = it_ec->second;
        if (cluster.size() == 0)
            continue;
        ret << (it_ec->first) << ": ";
        for (auto it = cluster.begin(); it != cluster.end(); ++it)
        {
            if (it != cluster.begin())
                ret << ", ";
            ret << (*it).string();
        }
        ret << std::endl;
    }
    return ret.str();
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


void proof_graph_t::enumerate_chains_of_grouped_axioms_from_node(
    node_idx_t from, std::list< std::list<edge_idx_t> > *out) const
{
    const kb::knowledge_base_t *kb = sys()->knowledge_base();
    const hash_set<hypernode_idx_t> *hns = search_hypernodes_with_node(from);
    if (hns == NULL) return;

    // ENUMERATE EDGES CONNECTED WITH GIVEN NODE
    std::list<edge_idx_t> edges;
    for (auto it = hns->begin(); it != hns->end(); ++it)
    {
        const hash_set<edge_idx_t> *_edges = search_edges_with_hypernode(*it);
        if (_edges == NULL) continue;

        for (auto it_e = _edges->begin(); it_e != _edges->end(); ++it_e)
        {
            const edge_t &e = edge(*it_e);
            if (e.tail() == (*it) and e.axiom_id() >= 0)
                edges.push_back(*it_e);
        }
    }

    if (not edges.empty())
    {
        // CREATE MAP OF EXCLUSIVENESS
        std::set< list_for_map<edge_idx_t> > exclusions;
        for (auto it1 = edges.begin(); it1 != edges.end(); ++it1)
        {
            const edge_t &e1 = edge(*it1);
            hash_set<axiom_id_t> grp = kb->search_axiom_group(e1.axiom_id());
            if (grp.empty()) continue;

            list_for_map<edge_idx_t> exc;
            for (auto it2 = edges.begin(); it2 != it1; ++it2)
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


void proof_graph_t::enumerate_chains_of_grouped_axioms_from_hypernode(
    hypernode_idx_t from, std::list< std::list<edge_idx_t> > *out) const
{
    const kb::knowledge_base_t *kb = sys()->knowledge_base();
    const hash_set<edge_idx_t> *edges = search_edges_with_hypernode(from);
    if (edges == NULL) return;

    std::set< list_for_map<edge_idx_t> > exclusions;

    // CREATE MAP OF EXCLUSIVENESS
    for (auto it1 = edges->begin(); it1 != edges->end(); ++it1)
    {
        const edge_t &e1 = edge(*it1);
        if (e1.tail() != from or e1.axiom_id() < 0) continue;

        hash_set<axiom_id_t> grp = kb->search_axiom_group(e1.axiom_id());
        if (grp.empty()) continue;

        list_for_map<edge_idx_t> exc;
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


std::list< std::vector<node_idx_t> >
proof_graph_t::enumerate_targets_of_chain(
    const lf::axiom_t &ax, bool is_backward, int max_depth) const
{
    std::vector<const literal_t*>
        lits = (is_backward ? ax.func.get_rhs() : ax.func.get_lhs());
    std::vector<std::string> arities;

    for (auto it = lits.begin(); it != lits.end(); ++it)
        arities.push_back((*it)->get_predicate_arity());

    return enumerate_nodes_list_with_arities(arities, max_depth);
}


std::list< std::vector<node_idx_t> >
proof_graph_t::enumerate_nodes_list_with_arities(
    const std::vector<std::string> &arities, int depth_limit) const
{
    std::vector< std::vector<node_idx_t> > candidates;
    std::list< std::vector<node_idx_t> > out;

    for (auto it = arities.begin(); it != arities.end(); ++it)
    {
        const hash_set<node_idx_t> *_idx = search_nodes_with_arity(*it);
        if (_idx == NULL) return out;

        std::vector<node_idx_t> _new;
        for (auto n = _idx->begin(); n != _idx->end(); ++n)
        {
            if (depth_limit < 0 or node(*n).depth() <= depth_limit)
                _new.push_back(*n);
        }

        if (_new.empty()) return out;
        candidates.push_back(_new);
    }

    std::vector<int> indices(arities.size(), 0);
    bool do_end_loop(false);

    while( not do_end_loop )
    {
        out.push_back(std::vector<node_idx_t>());
        for (int i = 0; i < candidates.size(); ++i)
        {
            node_idx_t idx = candidates.at(i).at(indices[i]);
            out.back().push_back(idx);
        }

        /* INCREMENT */
        ++indices[0];
        for (int i = 0; i < candidates.size(); ++i)
        {
            if (indices[i] >= candidates[i].size())
            {
                if( i < indices.size() - 1 )
                {
                    indices[i] = 0;
                    ++indices[i+1];
                }
                else do_end_loop = true;
            }
        }
    }

    return out;
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


template<class It> const hash_set<hypernode_idx_t>*
    proof_graph_t::find_hypernode_with_unordered_nodes(It begin, It end) const
{
    size_t hash = get_hash_of_nodes(std::list<node_idx_t>(begin, end));
    auto find = m_maps.unordered_nodes_to_hypernode.find(hash);
    return (find != m_maps.unordered_nodes_to_hypernode.end()) ?
        &(find->second) : NULL;
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

    auto it = m_maps.nodes_sub.find(t1);
    if (it == m_maps.nodes_sub.end())
        return -1;

    auto it2 = it->second.find(t2);
    if (it2 == it->second.end())
        return -1;

    return it2->second;
}


node_idx_t proof_graph_t::find_neg_sub_node(term_t t1, term_t t2) const
{
    if (t1 > t2) std::swap(t1, t2);

    auto it = m_maps.nodes_negsub.find(t1);
    if (it == m_maps.nodes_negsub.end())
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


void proof_graph_t::print( std::ostream *os ) const
{
    // PRINT NODES
    (*os) << "# node:" << std::endl;
    for( auto i = 0; i < m_nodes.size(); ++i )
    {
        (*os) << "    "
            << node(i).to_string()
            << format(":depth(%d)", node(i).depth())
            << std::endl;
    }

    // PRINT AXIOMS
    hash_set<axiom_id_t> set_axioms;
    std::list<axiom_id_t> list_axioms;
    for( auto it = m_edges.begin(); it != m_edges.end(); ++it )
        if( it->axiom_id() >= 0 )
            set_axioms.insert(it->axiom_id());
    list_axioms.assign( set_axioms.begin(), set_axioms.end() );
    list_axioms.sort();

    (*os) << "# axiom" << std::endl;
    for( auto ax = list_axioms.begin(); ax != list_axioms.end(); ++ax )
    {
        lf::axiom_t axiom = sys()->knowledge_base()->get_axiom(*ax);
        (*os) << "    " << axiom.id << ":" << axiom.name << ":"
              << axiom.func.to_string() << std::endl;
    }

    // PRINT EDGES
    (*os) << "# edge:" << std::endl;
    for( auto i = 0; i < m_edges.size(); ++i )
        (*os) << "    " << edge_to_string(i) << std::endl;

    int idx(0);
    std::string sub = m_vc_unifiable.to_string();
    while((idx = sub.find('\n', idx + 1)) >= 0)
        sub = sub.replace(idx, 1, "\n    ");

    // PRINT SUBSTITUSIONS
    (*os) << "# substitusions:" << std::endl;
    (*os) << "    " << sub << std::endl;

    const std::list<mutual_exclusion_t> &muexs = m_mutual_exclusions;

    // PRINT MUTUAL-EXCLUSIONS
    (*os) << "# mutual-exclusions:" << std::endl;
    for (auto it = muexs.begin(); it != muexs.end(); ++it)
    {
        (*os) << "    " << node(it->indices.first).to_string()
            << " _|_ " << node(it->indices.second).to_string()
            << " : " << it->unifier.to_string() << std::endl;
    }

    // PRINT EDGE'S CONDITIONS
    (*os) << "# conditions-for-edge:" << std::endl;
    for (int i = 0; i < m_edges.size(); ++i)
    {
        auto conds = m_subs_of_conditions_for_chain.find(i);
        if (conds == m_subs_of_conditions_for_chain.end()) continue;

        (*os) << "    edge[" << i << "]: ";
        for (auto p = conds->second.begin(); p != conds->second.end(); ++p)
        {
            if (p != conds->second.begin())
                (*os) << ", ";
            (*os) << "(= "
                << p->first.string() << " "
                << p->second.string() << ")";
        }
        (*os) << std::endl;
    }
}


node_idx_t proof_graph_t::add_node(
    const literal_t &lit, node_type_e type, int depth)
{
    node_t add(lit, type, m_nodes.size(), depth);
    node_idx_t out = m_nodes.size();
    
    m_nodes.push_back(add);
    m_maps.predicate_to_nodes[lit.predicate][lit.terms.size()].insert(out);
    m_maps.depth_to_nodes[depth].insert(out);
    
    if(lit.predicate == "=")
    {
        term_t t1(lit.terms[0]), t2(lit.terms[1]);
        if (t1 > t2) std::swap(t1, t2);

        if (lit.truth)
        {
            m_maps.nodes_sub[t1][t2] = out;
            if (t1.is_constant() and not t2.is_constant())
                m_maps.var_to_consts[t2].insert(t1);
            else if (t2.is_constant() and not t1.is_constant())
                m_maps.var_to_consts[t1].insert(t2);
        }
        else
            m_maps.nodes_negsub[t1][t2] = out;
    }
    
    for (unsigned i = 0; i < lit.terms.size(); i++)
    {
        const term_t& t = lit.terms.at(i);
        m_maps.term_to_nodes[t].insert(out);
    }

    return out;
}


hypernode_idx_t proof_graph_t::chain(
    hypernode_idx_t from, const lf::axiom_t &implication, bool is_backward )
{
    std::vector<literal_t> literals_to;
    hash_map<term_t, term_t> subs;
    hash_map<term_t, hash_set<term_t> > conds;

    {
        /* CREATE SUBSTITUTIONS */
        const std::vector<node_idx_t> &indices_from = hypernode(from);
        const lf::logical_function_t &lhs = implication.func.branch(0);
        const lf::logical_function_t &rhs = implication.func.branch(1);
        std::vector<const literal_t*>
            ax_to( ( is_backward ? lhs : rhs ).get_all_literals() ),
            ax_from( ( is_backward ? rhs : lhs ).get_all_literals() );

        get_substitutions_for_chain(indices_from, ax_from, &subs, &conds);
        literals_to.assign(ax_to.size(), literal_t());

        /* SUBSTITUTE TERMS */
        for (size_t i = 0; i < ax_to.size(); ++i)
        {
            literals_to[i] = *ax_to.at(i);
            for (size_t j = 0; j < literals_to[i].terms.size(); ++j)
            {
                term_t &term = literals_to[i].terms[j];
                term = substitute_term_for_chain(term, &subs);
            }
        }
    }

    int depth = get_depth_of_deepest_node(from);
    if (depth >= 0) ++depth;

    /* ADD NODES AND HYPERNODE */
    std::vector<node_idx_t> hypernode_to( literals_to.size(), -1 );
    for (size_t i = 0; i < literals_to.size(); ++i)
    {
        node_idx_t idx = add_node(literals_to[i], NODE_HYPOTHESIS, depth);
        hypernode_to[i] = idx;
    }
    hypernode_idx_t idx_hn_to = add_hypernode( hypernode_to );

    /* SET MASTER-HYPERNODE */
    for (auto it = hypernode_to.begin(); it != hypernode_to.end(); ++it)
        m_nodes[*it].set_master_hypernode(idx_hn_to);

    /* ADD EDGE */
    edge_type_e type = is_backward ? EDGE_HYPOTHESIZE : EDGE_IMPLICATION;
    edge_idx_t edge_idx =
        add_edge(edge_t(type, from, idx_hn_to, implication.id));

    /* ADD CONDITIONAL SUBS */
    if (not conds.empty())
    {
        std::list< std::pair<term_t, term_t> > *cd =
            &m_subs_of_conditions_for_chain[edge_idx];

        for (auto it = conds.begin(); it != conds.end(); ++it)
        for (auto t = it->second.begin(); t != it->second.end(); ++t)
            cd->push_back(std::make_pair(it->first, *t));
    }

    /* ADD AXIOM HISTORY */
    if (is_backward)
        m_maps.axiom_to_hypernodes_backward[implication.id].insert(from);
    else
        m_maps.axiom_to_hypernodes_forward[implication.id].insert(from);
    
    return idx_hn_to;
}


void proof_graph_t::get_substitutions_for_chain(
    const std::vector<node_idx_t> &nodes,
    const std::vector<const literal_t*> &lit,
    hash_map<term_t, term_t> *subs,
    hash_map<term_t, hash_set<term_t> > *conds) const
{
    assert(nodes.size() == lit.size());

    for (size_t i=0; i<nodes.size(); ++i)
    {
        const literal_t &li_ax = *(lit.at(i));
        const literal_t &li_hy = node(nodes.at(i)).literal();

        for (size_t j=0; j<li_ax.terms.size(); ++j)
        {
            const term_t &t_ax(li_ax.terms.at(j)), &t_hy(li_hy.terms.at(j));
            get_substitutions_for_chain_sub(t_ax, t_hy, subs, conds);

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

                get_substitutions_for_chain_sub(
                    term_t(s_ax.substr(0, idx1)), term_t(sub), subs, conds);
            }
        }
    }
}


void proof_graph_t::get_substitutions_for_chain_sub(
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


term_t proof_graph_t::substitute_term_for_chain(
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


int proof_graph_t::get_depth_of_deepest_node(hypernode_idx_t idx) const
{
    int out = -1;
    const std::vector<node_idx_t>& hn = hypernode(idx);
    for (auto it = hn.begin(); it != hn.end(); ++it)
    {
        int depth = node(*it).depth();
        if (depth > out) out = depth;
    }
    return out;
}


void proof_graph_t::generate_mutual_exclusion_for_inconsistency( node_idx_t idx )
{
    const kb::knowledge_base_t *kb = sys()->knowledge_base();
    const node_t &target = node(idx);
    std::string arity = target.literal().get_predicate_arity();
    std::list<axiom_id_t> axioms = kb->search_inconsistencies(arity);

    for (auto ax = axioms.begin(); ax != axioms.end(); ++ax)
    {
        lf::axiom_t axiom = kb->get_axiom(*ax);

        const literal_t &lit1 = axiom.func.branch(0).literal();
        const literal_t &lit2 = axiom.func.branch(1).literal();
        bool target_is_first = (lit1.get_predicate_arity() == arity);

        const hash_set<node_idx_t> *idx_nodes = target_is_first ?
            search_nodes_with_predicate(lit2.predicate, lit2.terms.size()) :
            search_nodes_with_predicate(lit1.predicate, lit1.terms.size());

        if (idx_nodes == NULL) continue;

        for (auto it = idx_nodes->begin(); it != idx_nodes->end(); ++it)
        {
            if (target_is_first)
                apply_inconsistency_sub(idx, *it, axiom);
            else
                apply_inconsistency_sub(*it, idx, axiom);
        }
    }
}


void proof_graph_t::apply_inconsistency_sub(
    node_idx_t i, node_idx_t j, const lf::axiom_t &axiom )
{
    unifier_t theta;
    const literal_t &inc_1 = axiom.func.branch(0).literal();
    const literal_t &inc_2 = axiom.func.branch(1).literal();
    const literal_t &target_1 = node(i).literal();
    const literal_t &target_2 = node(j).literal();

    for (unsigned t1 = 0; t1 < target_1.terms.size(); t1++)
    for (unsigned t2 = 0; t2 < target_2.terms.size(); t2++)
    {
        if (inc_1.terms.at(t1) == inc_2.terms.at(t2))
        {
            const term_t &term1 = target_1.terms.at(t1);
            const term_t &term2 = target_2.terms.at(t2);
            theta.add(term1, term2);
        }
    }

    IF_VERBOSE_FULL(
        "Inconsistent: " + node(i).to_string() + ", "
        + node(j).to_string() + theta.to_string() );
    
    m_maps.node_to_inconsistency[i].insert(axiom.id);
    m_maps.node_to_inconsistency[j].insert(axiom.id);

    mutual_exclusion_t mu = { std::make_pair(i, j), theta };
    m_mutual_exclusions.push_back(mu);
}



void proof_graph_t::generate_unification_assumptions(node_idx_t target)
{
    if (node(target).is_equality_node() or node(target).is_non_equality_node())
        return;

    std::list<node_idx_t> unifiables = enumerate_unifiable_nodes(target);

    /* UNIFY EACH UNIFIABLE NODE PAIR. */
    for (auto it = unifiables.begin(); it != unifiables.end(); ++it)
        _chain_for_unification(target, *it);
}


std::list<node_idx_t>
    proof_graph_t::enumerate_unifiable_nodes( node_idx_t target )
{
    const literal_t &lit = node(target).literal();
    auto candidates =
        search_nodes_with_predicate(lit.predicate, lit.terms.size());
    std::list<node_idx_t> unifiables;
    unifier_t unifier;

    for( auto it=candidates->begin(); it!=candidates->end(); ++it )
    {
        if( target == (*it) ) continue;

        node_idx_t n1 = target;
        node_idx_t n2 = (*it);
        if (n1 > n2) std::swap(n1, n2);

        // IGNORE THE PAIR WHICH HAS BEEN CONSIDERED ALREADY.
        if (not is_considered_unification(n1, n2))
        {
            m_logs.considered_unifications[n1].insert(n2);

            bool unifiable = check_unifiability(
                m_nodes[n1].literal(), m_nodes[n2].literal(),
                false, &unifier);
            if (unifiable and can_unify_nodes(n1, n2))
                unifiables.push_back(*it);
        }
    }
    return unifiables;
}


void proof_graph_t::_chain_for_unification(node_idx_t i, node_idx_t j)
{
    std::vector<node_idx_t> unified_nodes; // FROM
    std::vector<node_idx_t> unify_nodes; // TO
    unifier_t uni;

    unified_nodes.push_back(i);
    unified_nodes.push_back(j);

    if (not check_unifiability(node(i).literal(), node(j).literal(), false, &uni))
        return;

    /** CREATE UNIFICATION-NODES & UPDATE VARIABLES. */
    for (auto sub = uni.substitutions().begin();
        sub != uni.substitutions().end(); ++sub)
    {
        term_t t1(sub->terms[0]), t2(sub->terms[1]);
        if (t1 == t2) continue;

        node_idx_t sub_node_idx = find_sub_node(t1, t2);
        if (sub_node_idx < 0)
        {
            if (t1 > t2) std::swap(t1, t2);
            sub_node_idx = add_node(*sub, NODE_HYPOTHESIS, -1);
            m_maps.nodes_sub[t1][t2] = sub_node_idx;
            m_vc_unifiable.add(t1, t2);
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


void proof_graph_t::_add_nodes_of_transitive_unification( term_t t )
{
    const hash_set<term_t> *terms = m_vc_unifiable.find_cluster(t);
    assert( terms != NULL );

    for( auto it = terms->begin(); it != terms->end(); ++it )
    {
        if( t == (*it) ) continue;
        if( t.is_constant() and it->is_constant() ) continue;

        /* GENERATE TRANSITIVE UNIFICATION. */
        if( find_sub_node(t, *it) < 0 )
        {
            term_t t1(t), t2(*it);
            if(t1 > t2) std::swap(t1, t2);

            node_idx_t idx = add_node(
                literal_t("=", t1, t2), NODE_HYPOTHESIS, -1 );
            m_maps.nodes_sub[t1][t2] = idx;
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

    if( not do_ignore_truthment and p1.truth != p2.truth ) return false;
    if( p1.predicate    != p2.predicate    ) return false;
    if( p1.terms.size() != p2.terms.size() ) return false;

    for( int i=0; i<p1.terms.size(); i++ )
    {        
        if( p1.terms[i] != p2.terms[i] )
        {
            if( p1.terms[i].is_constant() and p2.terms[i].is_constant() )
                return false;
            else
                out->add( p1.terms[i], p2.terms[i] );
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
            m_maps.node_to_hypernode[*it].push_back(idx);

        size_t h = get_hash_of_nodes(
            std::list<node_idx_t>(indices.begin(), indices.end()));
        m_maps.unordered_nodes_to_hypernode[h].insert(idx);
    }
    
    return idx;
}


void proof_graph_t::
    generate_mutual_exclusion_for_counter_nodes( node_idx_t idx )
{
    const literal_t &l1 = node(idx).literal();
    const hash_set<node_idx_t>* indices =
        search_nodes_with_predicate(l1.predicate, l1.terms.size());
    for( auto it = indices->begin(); it != indices->end(); ++it )
    {
        if( *it == idx ) continue;

        const literal_t &l2 = node(*it).literal();
        if( l1.truth == l2.truth ) continue;

        mutual_exclusion_t mu;
        mu.indices = std::make_pair( idx, *it );
        bool unifiable = check_unifiability( l1, l2, true, &mu.unifier );

        if( unifiable )
            m_mutual_exclusions.push_back( mu );
    }
}


}

}

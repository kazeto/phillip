/* -*- coding: utf-8 -*- */

#ifndef INCLUDE_HENRY_PROOF_GRAPH_INLINE_H
#define INCLUDE_HENRY_PROOF_GRAPH_INLINE_H


#include <algorithm>


namespace phil
{

namespace pg
{


inline node_t::node_t(
    const literal_t &lit, node_type_e type, node_idx_t idx,
    int depth, const hash_set<node_idx_t> &ev)
    : m_type(type), m_literal(lit), m_index(idx), m_depth(depth),
    m_master_hypernode_idx(-1), m_evidences(ev)
{}


inline const hash_set<pg::node_idx_t>& node_t::evidences() const
{
    return m_evidences;
}


inline void node_t::set_master_hypernode(hypernode_idx_t idx)
{
    m_master_hypernode_idx = idx;
}


inline hypernode_idx_t node_t::master_hypernode() const
{
    return m_master_hypernode_idx;
}


inline bool node_t::is_equality_node() const
{
    return (m_literal.predicate == "=" and m_literal.truth);
}


inline bool node_t::is_non_equality_node() const
{
    return (m_literal.predicate == "=" and not m_literal.truth);
}


inline bool node_t::is_transitive_equality_node() const
{
    return is_equality_node() and m_master_hypernode_idx == -1;
}


inline std::string node_t::to_string() const
{
    return m_literal.to_string() + format(":%d", m_index);
}


inline edge_t::edge_t()
    : m_type(EDGE_UNDERSPECIFIED),
      m_index_tail(-1), m_index_head(-1), m_axiom_id(-1)
{}


inline edge_t::edge_t(
    edge_type_e type, hypernode_idx_t tail, hypernode_idx_t head, axiom_id_t id )
    : m_type(type), m_index_tail(tail), m_index_head(head), m_axiom_id(id)
{}


inline bool edge_t::is_chain_edge() const
{
    return type() == EDGE_HYPOTHESIZE or type() == EDGE_IMPLICATION;
}


inline bool edge_t::is_unify_edge() const
{
    return type() == EDGE_UNIFICATION;
}


inline unifier_t::unifier_t( const term_t &x, const term_t &y )
{ add(x, y); }


inline const term_t* unifier_t::find_substitution_term(const term_t &x) const
{
    auto found = m_mapping.find(x);
    return (found != m_mapping.end()) ? &(found->second) : NULL;
}


inline const std::set<literal_t>& unifier_t::substitutions() const
{
    return m_substitutions;
}


inline const hash_map<term_t, term_t>& unifier_t::mapping() const
{
    return m_mapping;
}


inline void unifier_t::add(term_t x, term_t y)
{
    if (x == y) return;
    if (x < y) std::swap(x, y);

    literal_t added("=", x, y);
    m_substitutions.insert(added);
    m_mapping[x] = y;
}


inline void unifier_t::clear()
{
    m_substitutions.clear();
    m_mapping.clear();
}


inline bool unifier_t::empty() const
{
    return m_substitutions.empty();
}


inline const hash_map<index_t, hash_set<term_t> >&
proof_graph_t::unifiable_variable_clusters_set_t::clusters() const
{ return m_clusters; }


inline const hash_set<term_t>* proof_graph_t
    ::unifiable_variable_clusters_set_t::find_cluster(term_t t) const
{
    auto it = m_map_v2c.find(t);
    return (it != m_map_v2c.end()) ? &m_clusters.at(it->second) : NULL;
}


inline bool
proof_graph_t::unifiable_variable_clusters_set_t::is_in_same_cluster(
    term_t t1, term_t t2 ) const
{
    auto i_v1 = m_map_v2c.find(t1);
    if( i_v1 == m_map_v2c.end() ) return false;
    auto i_v2 = m_map_v2c.find(t2);
    if( i_v2 == m_map_v2c.end() ) return false;
    return ( i_v1->second == i_v2->second );
}


inline proof_graph_t::proof_graph_t(phillip_main_t *main, const std::string &name)
: m_phillip(main), m_name(name), m_is_timeout(false)
{
}


inline node_idx_t proof_graph_t::
    add_observation(const literal_t &lit, int depth)
{
    int idx = add_node(lit, NODE_OBSERVABLE, depth, hash_set<node_idx_t>());
    std::list<std::tuple<node_idx_t, unifier_t, axiom_id_t> > muex;

    get_mutual_exclusions(lit, &muex);
    _generate_mutual_exclusions(idx, muex);
    _generate_unification_assumptions(idx);

    return idx;
}


inline hypernode_idx_t proof_graph_t::backward_chain(
    const std::vector<node_idx_t> &target, const lf::axiom_t &axiom)
{
    return chain(target, axiom, true);
}


inline hypernode_idx_t proof_graph_t::forward_chain(
    const std::vector<node_idx_t> &target, const lf::axiom_t &axiom)
{
    return chain(target, axiom, false);
}


inline const std::vector<node_t>& proof_graph_t::nodes() const
{ return m_nodes; }


inline const node_t& proof_graph_t::node( node_idx_t i ) const
{ return m_nodes.at(i); }


inline const std::vector<edge_t>& proof_graph_t::edges() const
{
    return m_edges;
}


inline const edge_t& proof_graph_t::edge( edge_idx_t i ) const
{
    return m_edges.at(i);
}


inline const std::vector< std::vector<node_idx_t> >&
    proof_graph_t::hypernodes() const
{
        return m_hypernodes;
}


inline const std::vector<node_idx_t>&
proof_graph_t::hypernode( hypernode_idx_t i ) const
{
    return m_hypernodes.at(i);
}


inline const std::vector<std::list<std::pair<literal_t, pg::node_idx_t> > >&
proof_graph_t::requirements() const
{
    return m_requirements;
}


inline const unifier_t* proof_graph_t::search_mutual_exclusion_of_node(
    node_idx_t n1, node_idx_t n2) const
{
    if (n1 > n2) std::swap(n1, n2);
    auto found1 = m_mutual_exclusive_nodes.find(n1);
    if (found1 == m_mutual_exclusive_nodes.end()) return NULL;

    auto found2 = found1->second.find(n2);
    if (found2 == found1->second.end()) return NULL;

    return &found2->second;
}


inline const hash_set<node_idx_t>*
proof_graph_t::search_nodes_with_term( term_t term ) const
{
    auto iter_tm = m_maps.term_to_nodes.find( term );
    return ( iter_tm != m_maps.term_to_nodes.end() ) ? &iter_tm->second : NULL;
}


inline const hash_set<node_idx_t>* proof_graph_t::search_nodes_with_predicate(
    predicate_t predicate, int arity ) const
{
    auto iter_nm = m_maps.predicate_to_nodes.find( predicate );
    if( iter_nm == m_maps.predicate_to_nodes.end() ) return NULL;

    auto iter_an = iter_nm->second.find( arity );
    if( iter_an == iter_nm->second.end() ) return NULL;

    return &iter_an->second;
}


inline const hash_set<node_idx_t>*
proof_graph_t::search_nodes_with_arity(const arity_t &arity) const
{
    int idx(arity.rfind('/')), num;
    assert( idx > 0 );
    _sscanf( arity.substr(idx+1).c_str(), "%d", &num );

    return search_nodes_with_predicate(arity.substr(0, idx), num);
}


inline const hash_set<node_idx_t>*
proof_graph_t::search_nodes_with_depth(int depth) const
{
    auto it = m_maps.depth_to_nodes.find( depth );
    return (it == m_maps.depth_to_nodes.end()) ? NULL : &it->second;
}


inline const hash_set<edge_idx_t>*
    proof_graph_t::search_edges_with_hypernode( hypernode_idx_t idx ) const
{
    auto it = m_maps.hypernode_to_edge.find(idx);
    return (it == m_maps.hypernode_to_edge.end()) ? NULL : &it->second;
}


inline const hash_set<hypernode_idx_t>*
proof_graph_t::search_hypernodes_with_node( node_idx_t node_idx ) const
{
    auto it = m_maps.node_to_hypernode.find( node_idx );
    return (it == m_maps.node_to_hypernode.end()) ? NULL : &it->second;
}


template<class It> const hash_set<hypernode_idx_t>*
proof_graph_t::find_hypernode_with_unordered_nodes(It begin, It end) const
{
    size_t hash = get_hash_of_nodes(std::list<node_idx_t>(begin, end));
    auto find = m_maps.unordered_nodes_to_hypernode.find(hash);
    return (find != m_maps.unordered_nodes_to_hypernode.end()) ?
        &(find->second) : NULL;
}


inline hypernode_idx_t
    proof_graph_t::find_parental_hypernode( hypernode_idx_t idx ) const
{
    edge_idx_t e = find_parental_edge(idx);
    return (e >= 0) ? edge(e).tail() : -1;
}


inline const hash_set<term_t>*
proof_graph_t::find_variable_cluster( term_t t ) const
{
    return m_vc_unifiable.find_cluster(t);
}


inline bool proof_graph_t::do_disregard_hypernode(hypernode_idx_t idx) const
{
    return m_hypernodes_disregarded.count(idx) > 0;
}


template <class ContainerPtr>
void proof_graph_t::erase_invalid_chain_candidates_with_coexistence(
    ContainerPtr ptr_cands) const
{
#ifndef DISABLE_CANCELING
    hash_map<node_idx_t, hash_map<node_idx_t, bool> > log;

    for (auto it = ptr_cands->begin(); it != ptr_cands->end();)
    {
        bool is_valid(true);
        const std::vector<node_idx_t> &ns = it->nodes;

        // IF ANY PAIR OF NODES IN EVIDENCE CANNOT CO-EXIST,
        // THE CHAIN FROM THEM IS INVALID.
        for (auto n1 = ns.begin(); n1 != ns.end() and is_valid; ++n1)
        for (auto n2 = ns.begin(); n2 != n1 and is_valid; ++n2)
        {
            auto find1 = log.find(*n1);
            if (find1 != log.end())
            {
                auto find2 = find1->second.find(*n2);
                if (find2 != find1->second.end())
                {
                    is_valid = find2->second;
                    continue;
                }
            }

            bool can_coexist = _check_nodes_coexistency(*n1, *n2);
            log[*n1][*n2] = log[*n2][*n1] = can_coexist;
            is_valid = can_coexist;
        }

        if (is_valid) ++it;
        else it = ptr_cands->erase(it);
    }
#endif
}


inline bool proof_graph_t::
is_hypernode_for_unification(hypernode_idx_t idx) const
{ return m_indices_of_unification_hypernodes.count(idx) > 0; }


inline void proof_graph_t::add_attribute(const std::string &name, const std::string &value)
{
    m_attributes[name] = value;
}


inline edge_idx_t proof_graph_t::add_edge( const edge_t &edge )
{
    m_maps.hypernode_to_edge[edge.head()].insert(m_edges.size());
    m_maps.hypernode_to_edge[edge.tail()].insert(m_edges.size());
    m_edges.push_back(edge);
    return m_edges.size() - 1;
}


inline bool proof_graph_t::_is_considered_unification(
    node_idx_t i, node_idx_t j ) const
{
    if (i > j) std::swap(i, j);

    hash_map<node_idx_t, hash_set<node_idx_t> >::const_iterator
        it1 = m_temporal.considered_unifications.find(i);
    if( it1 == m_temporal.considered_unifications.end() )
        return false;

    hash_set<node_idx_t>::const_iterator it2 = it1->second.find(j);
    return it2 != it1->second.end();
}


inline bool proof_graph_t::_is_considered_exclusion(
    node_idx_t i, node_idx_t j) const
{
    if (i > j) std::swap(i, j);

    hash_map<node_idx_t, hash_set<node_idx_t> >::const_iterator
        it1 = m_temporal.considered_exclusions.find(i);
    if (it1 == m_temporal.considered_exclusions.end())
        return false;

    hash_set<node_idx_t>::const_iterator it2 = it1->second.find(j);
    return it2 != it1->second.end();
}


inline int proof_graph_t::get_depth_of_deepest_node(hypernode_idx_t idx) const
{
    return get_depth_of_deepest_node(hypernode(idx));
}


}
    
}


#endif

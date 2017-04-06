/* -*- coding: utf-8 -*- */

#ifndef INCLUDE_HENRY_PROOF_GRAPH_INLINE_H
#define INCLUDE_HENRY_PROOF_GRAPH_INLINE_H


#include <algorithm>


namespace dav
{

namespace pg
{


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


inline node_idx_t proof_graph_t::
    add_observation(const atom_t &lit, int depth)
{
    int idx = add_node(lit, NODE_OBSERVABLE, depth, hash_set<node_idx_t>());
    std::list<std::tuple<node_idx_t, unifier_t> > muex;

    get_mutual_exclusions(lit, &muex);
    _generate_mutual_exclusions(idx, muex);
    _generate_unification_assumptions(idx);
    m_observations.insert(idx);

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
proof_graph_t::hypernode(hypernode_idx_t i) const
{
    static const std::vector<node_idx_t> empty;
    return (i >= 0) ? m_hypernodes.at(i) : empty;
}


inline const hash_set<node_idx_t>& proof_graph_t::observation_indices() const
{
    return m_observations;
}


inline const std::vector<requirement_t>& proof_graph_t::requirements() const
{
    return m_requirements;
}


inline const unifier_t* proof_graph_t::search_mutual_exclusion_of_node(
    node_idx_t n1, node_idx_t n2) const
{
    return m_mutual_exclusive_nodes.find(n1, n2);
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
proof_graph_t::search_nodes_with_pid(const kb::predicate_id_t arity) const
{
    auto found = m_maps.pid_to_nodes.find(arity);
    return (found != m_maps.pid_to_nodes.end()) ? &found->second : NULL;
}


inline const hash_set<node_idx_t>*
proof_graph_t::search_nodes_with_same_predicate_as(const atom_t &lit) const
{
    if (lit.pid() != kb::INVALID_PREDICATE_ID)
        return search_nodes_with_pid(lit.pid());
    else
        return search_nodes_with_predicate(lit.predicate(), lit.terms().size());
}


inline const hash_set<node_idx_t>*
proof_graph_t::search_nodes_with_depth(depth_t depth) const
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


inline const hash_set<edge_idx_t>*
proof_graph_t::search_edges_with_node_in_head(node_idx_t idx) const
{
    auto found = m_maps.head_node_to_edges.find(idx);
    return (found == m_maps.head_node_to_edges.end()) ? NULL : &found->second;
}


inline const hash_set<edge_idx_t>*
proof_graph_t::search_edges_with_node_in_tail(node_idx_t idx) const
{
    auto found = m_maps.tail_node_to_edges.find(idx);
    return (found == m_maps.tail_node_to_edges.end()) ? NULL : &found->second;
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


template <class IterNodesArray>
bool proof_graph_t::check_nodes_coexistability(IterNodesArray begin, IterNodesArray end) const
{
#ifdef DISABLE_CANCELING
    return true;
#else
    for (auto n1 = begin; n1 != end; ++n1)
    for (auto n2 = begin; n2 != n1; ++n2)
    if (not _check_nodes_coexistability(*n1, *n2))
        return false;
    return true;
#endif
}


inline bool proof_graph_t::
is_hypernode_for_unification(hypernode_idx_t idx) const
{ return m_indices_of_unification_hypernodes.count(idx) > 0; }


inline void proof_graph_t::add_attribute(const std::string &name, const std::string &value)
{
    m_attributes[name] = value;
}


inline bool proof_graph_t::_is_considered_unification(
    node_idx_t i, node_idx_t j ) const
{
    return m_temporal.considered_unifications.count(i, j) > 0;
}


inline int proof_graph_t::get_depth_of_deepest_node(hypernode_idx_t idx) const
{
    return get_depth_of_deepest_node(hypernode(idx));
}


}
    
}


#endif

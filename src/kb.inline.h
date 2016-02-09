#pragma once


namespace phil
{

namespace kb
{


inline lf::axiom_t knowledge_base_t::get_axiom(axiom_id_t id) const
{
    if (id >= 0 and id < axioms.num_axioms())
        return axioms.get(id);
    else
        return lf::axiom_t();
}


inline std::list<axiom_id_t> knowledge_base_t::
search_axioms_with_rhs(const std::string &rhs) const
{
    predicate_id_t id = predicates.pred2id(rhs);
    return search_id_list(id, &m_cdb_rhs);
}


inline std::list<axiom_id_t> knowledge_base_t::
search_axioms_with_lhs(const std::string &lhs) const
{
    predicate_id_t id = predicates.pred2id(lhs);
    return search_id_list(lhs, &m_cdb_lhs);
}


inline const std::list<std::pair<term_idx_t, term_idx_t> >* knowledge_base_t::
search_inconsistent_terms(predicate_id_t a1, predicate_id_t a2) const
{
    return predicates.find_inconsistent_terms(a1, a2);
}


inline float knowledge_base_t::get_distance(const lf::axiom_t &axiom) const
{
    return (*m_distance_provider.instance)(axiom);
}


inline float knowledge_base_t::get_distance(axiom_id_t id) const
{
    return get_distance(get_axiom(id));
}


inline version_e knowledge_base_t::version() const
{
    return m_version;
}


inline bool knowledge_base_t::is_valid_version() const
{
    return m_version == KB_VERSION_12;
}


inline bool knowledge_base_t::is_writable() const
{
    return m_state == STATE_COMPILE;
}


inline bool knowledge_base_t::is_readable() const
{
    return m_state == STATE_QUERY;
}


inline int knowledge_base_t::num_of_axioms() const
{
    return axioms.num_axioms();
}


inline float knowledge_base_t::get_max_distance() const
{
    return m_config_for_compile.max_distance;
}


inline const std::string& knowledge_base_t::filename() const
{
    return m_filename;
}


inline void knowledge_base_t::clear_distance_cache()
{
    std::lock_guard<std::mutex> lock(ms_mutex_for_cache);
    m_cache_distance.clear();
}


inline std::string knowledge_base_t::axioms_database_t::get_name_of_unnamed_axiom()
{
    char buf[128];
    _sprintf(buf, "_%#.8lx", m_num_unnamed_axioms++);
    return std::string(buf);
}


inline const std::vector<predicate_with_arity_t>& knowledge_base_t::predicate_database_t::arities() const
{
    return m_arities;
}


inline predicate_id_t knowledge_base_t::predicate_database_t::pred2id(const predicate_with_arity_t &arity) const
{
    auto found = m_arity2id.find(arity);
    return (found != m_arity2id.end()) ? found->second : INVALID_PREDICATE_ID;
}


inline const predicate_with_arity_t& knowledge_base_t::predicate_database_t::id2pred(predicate_id_t id) const
{
    return (id < m_arities.size()) ? m_arities.at(id) : m_arities.front();    
}


inline const hash_map<predicate_id_t, functional_predicate_configuration_t>&
knowledge_base_t::predicate_database_t::functional_predicates() const
{
    return m_functional_predicates;
}


inline const functional_predicate_configuration_t*
knowledge_base_t::predicate_database_t::find_functional_predicate(predicate_id_t id) const
{
    auto found = m_functional_predicates.find(id);
    return (found != m_functional_predicates.end()) ? &found->second : nullptr;
}


inline const functional_predicate_configuration_t*
knowledge_base_t::predicate_database_t::find_functional_predicate(const predicate_with_arity_t &p) const
{
    return find_functional_predicate(pred2id(p));
}


inline const std::list<std::pair<term_idx_t, term_idx_t> >*
knowledge_base_t::predicate_database_t::find_inconsistent_terms(predicate_id_t a1, predicate_id_t a2) const
{
    assert(a1 <= a2);

    auto found1 = m_mutual_exclusions.find(a1);
    if (found1 != m_mutual_exclusions.end())
    {
        auto found2 = found1->second.find(a2);
        if (found2 != found1->second.end())
            return &found2->second;
    }
    return NULL;
}

    
inline bool knowledge_base_t::reachable_matrix_t::is_writable() const
{
    return (m_fout != NULL);
}


inline bool knowledge_base_t::reachable_matrix_t::is_readable() const
{
    return (m_fin != NULL);
}


}

}

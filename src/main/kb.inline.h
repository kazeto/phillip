#pragma once


namespace phil
{

namespace kb
{



inline std::list<axiom_id_t> knowledge_base_t::axioms_database_t::
gets_by_rhs(predicate_id_t rhs) const
{
    return gets_from_cdb((char*)&rhs, sizeof(predicate_id_t), &m_cdb_rhs);
}


inline std::list<axiom_id_t> knowledge_base_t::axioms_database_t::
gets_by_rhs(const predicate_with_arity_t &rhs) const
{
    return gets_by_rhs(kb()->predicates.pred2id(rhs));
}


inline std::list<axiom_id_t> knowledge_base_t::axioms_database_t::
gets_by_lhs(predicate_id_t lhs) const
{
    return gets_from_cdb((char*)&lhs, sizeof(predicate_id_t), &m_cdb_lhs);
}


inline std::list<axiom_id_t> knowledge_base_t::axioms_database_t::
gets_by_lhs(const predicate_with_arity_t &lhs) const
{
    return gets_by_lhs(kb()->predicates.pred2id(lhs));
}


inline float knowledge_base_t::get_distance(const lf::axiom_t &axiom) const
{
    return (*m_distance_provider.instance)(axiom);
}


inline float knowledge_base_t::get_distance(axiom_id_t id) const
{
    return get_distance(axioms.get(id));
}


inline float knowledge_base_t::get_max_distance() const
{
    return m_config_for_compile.max_distance;
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


inline const std::vector<predicate_with_arity_t>& knowledge_base_t::predicate_library_t::arities() const
{
    return m_arities;
}


inline predicate_id_t knowledge_base_t::predicate_library_t::pred2id(const predicate_with_arity_t &arity) const
{
    auto found = m_arity2id.find(arity);
    return (found != m_arity2id.end()) ? found->second : INVALID_PREDICATE_ID;
}


inline const predicate_with_arity_t& knowledge_base_t::predicate_library_t::id2pred(predicate_id_t id) const
{
    return (id < m_arities.size()) ? m_arities.at(id) : m_arities.front();    
}


inline const hash_map<predicate_id_t, predicate_property_t>&
knowledge_base_t::predicate_library_t::functional_predicates() const
{
    return m_functional_predicates;
}


inline const predicate_property_t*
knowledge_base_t::predicate_library_t::
find_functional_predicate(predicate_id_t id) const
{
    auto found = m_functional_predicates.find(id);
    return (found != m_functional_predicates.end()) ? &found->second : nullptr;
}


inline const predicate_property_t*
knowledge_base_t::predicate_library_t::
find_functional_predicate(const predicate_with_arity_t &p) const
{
    return find_functional_predicate(pred2id(p));
}


inline bool knowledge_base_t::predicate_library_t::
is_functional(predicate_id_t id) const
{
    return find_functional_predicate(id) != nullptr;
}

inline bool knowledge_base_t::predicate_library_t::
is_functional(const predicate_with_arity_t &p) const
{
    return find_functional_predicate(p) != nullptr;
}

inline const std::list<std::pair<term_idx_t, term_idx_t> >*
knowledge_base_t::predicate_library_t::find_inconsistent_terms(predicate_id_t a1, predicate_id_t a2) const
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



}

}

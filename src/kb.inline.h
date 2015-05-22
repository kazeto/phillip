#pragma once


namespace phil
{

namespace kb
{



inline float knowledge_base_t::get_max_distance()
{
    return ms_max_distance;
}


inline lf::axiom_t knowledge_base_t::get_axiom(axiom_id_t id) const
{
    if (id >= 0 and id < m_axioms.num_axioms())
        return m_axioms.get(id);
    else
        return lf::axiom_t();
}


inline std::list<axiom_id_t> knowledge_base_t::
search_axioms_with_rhs(const std::string &rhs) const
{
    arity_id_t id = m_arity_db.arity2id(rhs);
    return search_id_list(id, &m_cdb_rhs);
}


inline std::list<axiom_id_t> knowledge_base_t::
search_axioms_with_lhs(const std::string &lhs) const
{
    arity_id_t id = m_arity_db.arity2id(lhs);
    return search_id_list(lhs, &m_cdb_lhs);
}


inline const std::list<std::pair<term_idx_t, term_idx_t> >* knowledge_base_t::
search_inconsistent_terms(arity_id_t a1, arity_id_t a2) const
{
    return m_arity_db.find_inconsistent_terms(a1, a2);
}


inline float knowledge_base_t::get_distance(const lf::axiom_t &axiom) const
{
    return (*m_distance_provider.instance)(axiom);
}


inline float knowledge_base_t::get_distance(axiom_id_t id) const
{
    return get_distance(get_axiom(id));
}


inline float knowledge_base_t::get_soft_unifying_cost(
    const std::string &arity1, const std::string &arity2) const
{
    return m_category_table.instance->get(arity1, arity2);
}


inline bool knowledge_base_t::do_target_on_category_table(const arity_t &arity) const
{
    return m_category_table.instance->do_target(arity);
}


inline version_e knowledge_base_t::version() const
{
    return m_version;
}


inline bool knowledge_base_t::is_valid_version() const
{
    return m_version == KB_VERSION_7;
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
    return m_axioms.num_axioms();
}


inline const hash_set<std::string>& knowledge_base_t::stop_words() const
{
    return m_stop_words;
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


inline arity_id_t knowledge_base_t::search_arity_id(const arity_t &arity) const
{
    return m_arity_db.arity2id(arity);
}


inline const arity_t& knowledge_base_t::search_arity(arity_id_t id) const
{
    return m_arity_db.id2arity(id);
}


const unification_postponement_t* knowledge_base_t::
find_unification_postponement(arity_id_t arity) const
{
    return m_arity_db.find_unification_postponement(arity);
}


const unification_postponement_t* knowledge_base_t::
find_unification_postponement(const arity_t &arity) const
{
    return m_arity_db.find_unification_postponement(m_arity_db.arity2id(arity));
}


inline bool knowledge_base_t::axioms_database_t::is_writable() const
{
    return (m_fo_idx != NULL) and (m_fo_dat != NULL);
}


inline bool knowledge_base_t::axioms_database_t::is_readable() const
{
    return (m_fi_idx != NULL) and (m_fi_dat != NULL);
}


inline std::string knowledge_base_t::axioms_database_t::get_name_of_unnamed_axiom()
{
    char buf[128];
    _sprintf(buf, "_%#.8lx", m_num_unnamed_axioms++);
    return std::string(buf);
}


arity_id_t knowledge_base_t::arity_database_t::add(const arity_t &arity)
{
    auto found = m_arity2id.find(arity);

    if (found != m_arity2id.end())
        return found->second;
    else
    {
        arity_id_t id = m_arities.size();
        m_arity2id[arity] = id;
        m_arities.push_back(arity);
        return id;
    }
}


inline void knowledge_base_t::arity_database_t::add_unification_postponement(
    const unification_postponement_t &unipp)
{
    if (unipp.arity_id() != INVALID_ARITY_ID)
        m_unification_postponements[unipp.arity_id()] = unipp;
}


inline const std::vector<arity_t>& knowledge_base_t::arity_database_t::arities() const
{
    return m_arities;
}


inline arity_id_t knowledge_base_t::arity_database_t::arity2id(const arity_t &arity) const
{
    auto found = m_arity2id.find(arity);
    return (found != m_arity2id.end()) ? found->second : INVALID_ARITY_ID;
}


inline const arity_t& knowledge_base_t::arity_database_t::id2arity(arity_id_t id) const
{
    return (id < m_arities.size()) ? m_arities.at(id) : m_arities.front();    
}


inline const unification_postponement_t*
knowledge_base_t::arity_database_t::find_unification_postponement(arity_id_t id) const
{
    auto found = m_unification_postponements.find(id);
    return (found != m_unification_postponements.end()) ? &found->second : NULL;
}


inline const std::list<std::pair<term_idx_t, term_idx_t> >*
knowledge_base_t::arity_database_t::find_inconsistent_terms(arity_id_t a1, arity_id_t a2) const
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

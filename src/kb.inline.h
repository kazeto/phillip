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
    return search_id_list(rhs, &m_cdb_rhs);
}


inline std::list<axiom_id_t> knowledge_base_t::
search_axioms_with_lhs(const std::string &lhs) const
{
    return search_id_list(lhs, &m_cdb_lhs);
}


inline std::list<axiom_id_t> knowledge_base_t::
search_inconsistencies(const std::string &arity) const
{
    return search_id_list(arity, &m_cdb_inc_pred);
}


inline float knowledge_base_t::get_distance(const lf::axiom_t &axiom) const
{
    return (*m_rm_dist)(axiom);
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
    return m_version == KB_VERSION_5;
}


inline int knowledge_base_t::num_of_axioms() const
{
    return m_axioms.num_axioms();
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


inline size_t knowledge_base_t::search_arity_id(const std::string &arity) const
{
    size_t size;
    const size_t *get;
    get = (const size_t*)m_cdb_rm_idx.get(arity.c_str(), arity.size(), &size);

    return (get != NULL) ? (*get) : INVALID_ARITY_ID;
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

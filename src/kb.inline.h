#pragma once


namespace phil
{

namespace kb
{


inline float knowledge_base_t::get_max_distance() const
{
    return m_max_distance;
}


inline size_t knowledge_base_t::get_axiom_num() const
{
    return m_num_compiled_axioms + m_num_temporary_axioms;
}


inline size_t knowledge_base_t::get_compiled_axiom_num() const
{
    return m_num_compiled_axioms;
}


inline size_t knowledge_base_t::get_temporal_axiom_num() const
{
    return m_num_temporary_axioms;
}


inline std::list<axiom_id_t> knowledge_base_t::
search_axioms_with_rhs(const std::string &rhs) const
{
    return search_id_list(rhs, &m_cdb_rhs, &m_rhs_to_tmp_axioms);
}


inline std::list<axiom_id_t> knowledge_base_t::
search_axioms_with_lhs(const std::string &lhs) const
{
    return search_id_list(lhs, &m_cdb_lhs, &m_lhs_to_tmp_axioms);
}


inline std::list<axiom_id_t> knowledge_base_t::
search_inconsistencies(const std::string &arity) const
{
    return search_id_list(arity, &m_cdb_inc_pred, &m_inc_to_tmp_axioms);
}


inline const size_t* knowledge_base_t::
    search_arity_index(const std::string &arity) const
{
    size_t size;
    const size_t *get1;
    get1 = (const size_t*)
        m_cdb_rm_idx.get(arity.c_str(), arity.size(), &size);

    return get1;
}


inline std::string knowledge_base_t::_get_name_of_unnamed_axiom()
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

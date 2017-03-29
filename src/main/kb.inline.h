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


}

}

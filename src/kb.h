#pragma once

#include <iostream>
#include <fstream>
#include <map>
#include <list>
#include <string>

#include "./define.h"
#include "./logical_function.h"


namespace phil
{

namespace kb
{


/** This class define distance between predicates
 *  on creation of reachable-matrix. */
class distance_provider_t
{
public:
    virtual float operator() (
        float d, const std::string &a0,
        const std::string &a1, const std::string &a2,
        const lf::axiom_t &ax) const = 0;
};


class basic_distance_provider_t : public distance_provider_t
{
public:
    virtual float operator() (
        float d, const std::string &a0,
        const std::string &a1, const std::string &a2,
        const lf::axiom_t &ax) const
    { return d + 1.0f; }
};


class knowledge_base_t
{
public:
    static inline void max_distance(float dist) { ms_max_distance = dist; }
    static inline float max_distance() { return ms_max_distance; }

    knowledge_base_t(const std::string &filename);
    ~knowledge_base_t();

    /** Initialize knowledge base and
     *  prepare for compiling knowledge base. */
    void prepare_compile();

    /** Prepare for reading knowledge base. */
    void prepare_query();

    /** Call this method on end of compiling or reading knowledge base. */
    void finalize();

    /** Insert axiom into knowledge base as compiled axiom.
     *  This method can be called only in compile-mode. */
    void insert_implication_for_compile(
        const lf::logical_function_t &lf, std::string name);
    void insert_inconsistency_for_compile(
        const lf::logical_function_t &lf, std::string name);

    /** Insert axiom into knowledge base as temporal axiom.
     *  This method can be called anytime but
     *  you cannot add new axioms to compile after calling this method. */
    void insert_implication_temporary(
        const lf::logical_function_t &lf, std::string name);
    void insert_inconsistency_temporary(
        const lf::logical_function_t &lf, std::string name);

    inline size_t get_axiom_num() const;
    inline size_t get_compiled_axiom_num() const;
    inline size_t get_temporal_axiom_num() const;

    lf::axiom_t get_axiom(axiom_id_t id) const;
    inline std::list<axiom_id_t>
        search_axioms_with_rhs(const std::string &arity) const;
    inline std::list<axiom_id_t>
        search_axioms_with_lhs(const std::string &arity) const;
    inline std::list<axiom_id_t>
        search_inconsistencies(const std::string &arity) const;
    hash_set<axiom_id_t> search_axiom_group(axiom_id_t id) const;

    /** Return ditance between arity1 and arity2
     *  in a reachable-matrix in the current knowledge-base.
     *  If these arities are not reachable, then return -1. */
    float get_distance(
        const std::string &arity1, const std::string &arity2) const;

    /** Set new distance-provider.
     *  This function-object is used in making reachable-matrix. */
    void set_distance_provider(distance_provider_t *ptr);

private:
    /** A class of reachable-matrix.
     *  This is implemented with sqlite3. */
    class reachable_matrix_t
    {
    public:
        reachable_matrix_t(const std::string &filename, bool m_is_triangular);
        ~reachable_matrix_t();
        void prepare_compile();
        void prepare_query();
        void finalize();
        void put(size_t idx1, const hash_map<size_t, float> &dist);
        float get(size_t idx1, size_t idx2) const;
        hash_set<float> get(size_t idx) const;

        inline bool is_writable() const;
        inline bool is_readable() const;
        inline bool is_triangular() const;

    private:
        typedef unsigned long long pos_t;
        std::string   m_filename;
        std::ofstream *m_fout;
        std::ifstream *m_fin;
        hash_map<size_t, pos_t> m_map_idx_to_pos;
        bool m_is_triangular;
    };

    enum kb_state_e { STATE_NULL, STATE_COMPILE, STATE_QUERY };

    void _insert_cdb(
        const std::string &name, const lf::logical_function_t &lf);
    void _insert_cdb(
        const hash_map<std::string, hash_set<axiom_id_t> > &ids,
        cdb_data_t *dat);
    void _insert_axiom_temporary(
        const lf::logical_function_t &lf, std::string name);
    bool _can_insert_axiom_to_compile() const;
    void insert_arity(const std::string &arity);
    void insert_axiom_group_to_cdb();

    void create_reachable_matrix();
    void create_reachable_matrix_sub(
        const std::string &arity, hash_map<size_t, float> *out);

    /** Search axioms corresponding with given query.
     *  @param dat A database of cdb to seach axiom.
     *  @param tmp A map of temporal axioms related with dat. */
    std::list<axiom_id_t> search_id_list(
        const std::string &query, const cdb_data_t *dat,
        const hash_map<std::string, hash_set<axiom_id_t> > *tmp) const;

    inline const size_t* search_arity_index(const std::string &arity) const;

    inline std::string _get_name_of_unnamed_axiom();

    static float ms_max_distance;

    kb_state_e m_state;
    std::string m_filename;

    cdb_data_t m_cdb_id, m_cdb_name, m_cdb_rhs, m_cdb_lhs;
    cdb_data_t m_cdb_inc_pred, m_cdb_axiom_group;
    cdb_data_t m_cdb_rm_idx;
    reachable_matrix_t m_rm;
    
    size_t m_num_compiled_axioms;
    size_t m_num_temporary_axioms;
    size_t m_num_unnamed_axioms;

    /** Axioms which were added temporally. */
    hash_map<axiom_id_t, lf::axiom_t> m_temporary_axioms;
    hash_map<std::string, hash_set<axiom_id_t> >
        m_lhs_to_tmp_axioms, m_rhs_to_tmp_axioms, m_inc_to_tmp_axioms;

    /** All arities in this knowledge-base.
     *  This variable is used in constructing reachable-matrix. */
    hash_set<std::string> m_arity_set;

    hash_map<std::string, hash_set<axiom_id_t> >
        m_name_to_axioms, m_lhs_to_axioms, m_rhs_to_axioms, m_inc_to_axioms, m_group_to_axioms;

    distance_provider_t *m_rm_dist;
};



}

}

#include "./kb.inline.h"

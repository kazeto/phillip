#pragma once

#include <iostream>
#include <fstream>
#include <map>
#include <list>
#include <string>
#include <memory>

#include "./define.h"
#include "./logical_function.h"


namespace phil
{


namespace pg { class proof_graph_t; }


namespace kb
{


enum distance_provider_type_e
{
    DISTANCE_PROVIDER_UNDERSPECIFIED,
    DISTANCE_PROVIDER_BASIC,
    DISTANCE_PROVIDER_COST_BASED
};


enum unification_postpone_argument_type_e
{
    UNI_PP_INDISPENSABLE,           /// Is expressed as '*'.
    UNI_PP_INDISPENSABLE_PARTIALLY, /// Is expressed as '+'.
    UNI_PP_DISPENSABLE,             /// Is expressed as '.'.
};


/** This class define distance between predicates
 *  on creation of reachable-matrix. */
class distance_provider_t
{
public:
    virtual ~distance_provider_t() {}
    virtual float operator() (const lf::axiom_t &ax) const = 0;

    virtual distance_provider_type_e type() const
    { return DISTANCE_PROVIDER_UNDERSPECIFIED; }
};


class unification_postponement_t
{
public:
    unification_postponement_t() {}
    unification_postponement_t(
        const std::string &arity, const std::vector<char> &args,
        int num_for_partial_indispensability);

    bool do_postpone(const pg::proof_graph_t*, index_t n1, index_t n2) const;
    inline bool empty() const { return m_args.empty(); }

private:
    std::string m_arity;
    std::vector<char> m_args;
    int m_num_for_partial_indispensability;
};


/** A class of knowledge-base. */
class knowledge_base_t
{
public:
    struct deleter
    {
        void operator()(knowledge_base_t const* const p) const { delete p; }
    };

    static knowledge_base_t* instance();
    static void setup(std::string filename, distance_provider_type_e dist_type, float max_distance);
    static inline float get_max_distance();

    ~knowledge_base_t();

    /** Initializes knowledge base and
     *  prepares for compiling knowledge base. */
    void prepare_compile();

    /** Prepares for reading knowledge base. */
    void prepare_query();

    /** Call this method on end of compiling or reading knowledge base. */
    void finalize();

    /** Inserts new axiom into knowledge base as compiled axiom.
     *  This method can be called only in compile-mode. */
    void insert_implication(
        const lf::logical_function_t &lf, std::string name);
    
    /** Inserts new inconsistency into knowledge base as compiled axiom.
     *  This method can be called only in compile-mode. */
    void insert_inconsistency(const lf::logical_function_t &lf, std::string name);

    void insert_unification_postponement(const lf::logical_function_t &lf, std::string name);

    inline size_t get_axiom_num() const;

    lf::axiom_t get_axiom(axiom_id_t id) const;
    inline std::list<axiom_id_t> search_axioms_with_rhs(const std::string &arity) const;
    inline std::list<axiom_id_t> search_axioms_with_lhs(const std::string &arity) const;
    inline std::list<axiom_id_t> search_inconsistencies(const std::string &arity) const;
    hash_set<axiom_id_t> search_axiom_group(axiom_id_t id) const;
    unification_postponement_t get_unification_postponement(const std::string &arity) const;

    /** Returns ditance between arity1 and arity2
     *  in a reachable-matrix in the current knowledge-base.
     *  If these arities are not reachable, then return -1. */
    float get_distance(
        const std::string &arity1, const std::string &arity2) const;

    /** Returns distance between arity1 and arity2 with distance-provider. */
    inline float get_distance(const lf::axiom_t &axiom) const;


private:

    /** A class of reachable-matrix for all predicate pairs. */
    class global_reachable_matrix_t
    {
    public:
        global_reachable_matrix_t(const std::string &filename);
        ~global_reachable_matrix_t();
        void prepare_compile();
        void prepare_query();
        void finalize();
        void put(size_t idx1, const hash_map<size_t, float> &dist);
        float get(size_t idx1, size_t idx2) const;
        hash_set<float> get(size_t idx) const;

        inline bool is_writable() const;
        inline bool is_readable() const;

    private:
        typedef unsigned long long pos_t;
        std::string   m_filename;
        std::ofstream *m_fout;
        std::ifstream *m_fin;
        hash_map<size_t, pos_t> m_map_idx_to_pos;
    };

    enum kb_state_e { STATE_NULL, STATE_COMPILE, STATE_QUERY };

    knowledge_base_t(
        const std::string &filename, distance_provider_type_e dist);

    void write_config(const char *filename) const;
    void read_config(const char *filename);


    void _insert_cdb(
        const std::string &name, const lf::logical_function_t &lf);
    void _insert_cdb(
        const hash_map<std::string, hash_set<axiom_id_t> > &ids,
        cdb_data_t *dat);
    void insert_arity(const std::string &arity);

    /** Outputs m_group_to_axioms to m_cdb_axiom_group. */
    void insert_axiom_group_to_cdb();

    /** Creates reachable matrix.
     *  This is a sub-routine of finalize. */
    void create_reachable_matrix();
    
    void _create_reachable_matrix_direct(
        const hash_set<std::string> &arities,
        hash_map<size_t, hash_map<size_t, float> > *out);
    void _create_reachable_matrix_indirect(
        size_t key, hash_map<size_t, hash_map<size_t, float> > &base,
        hash_map<size_t, float> *out);

    void extend_inconsistency();
    void _enumerate_deducible_literals(
        const literal_t &target, hash_set<literal_t> *out) const;

    /** Returns axioms corresponding with given query.
     *  @param dat A database of cdb to seach axiom.
     *  @param tmp A map of temporal axioms related with dat. */
    std::list<axiom_id_t> search_id_list(
        const std::string &query, const cdb_data_t *dat) const;

    /** Returns index of given arity in reachable-matrix.
     *  On calling this method, ~.rm.idx.cdb must be readable. */
    inline const size_t* search_arity_index(const std::string &arity) const;

    /** Sets new distance-provider.
     *  This object is used in making reachable-matrix. */
    void set_distance_provider(distance_provider_type_e);

    inline std::string _get_name_of_unnamed_axiom();

    static std::unique_ptr<knowledge_base_t, deleter> ms_instance;
    static std::string ms_filename;
    static distance_provider_type_e ms_distance_provider_type;
    static float ms_max_distance;

    kb_state_e m_state;
    std::string m_filename;

    cdb_data_t m_cdb_id, m_cdb_name, m_cdb_rhs, m_cdb_lhs;
    cdb_data_t m_cdb_inc_pred, m_cdb_axiom_group, m_cdb_uni_pp;
    cdb_data_t m_cdb_rm_idx;
    global_reachable_matrix_t m_rm;
    
    size_t m_num_compiled_axioms;
    size_t m_num_unnamed_axioms;

    hash_map<size_t, hash_map<size_t, float> > m_partial_reachable_matrix;

    /** All arities in this knowledge-base.
     *  This variable is used on constructing reachable-matrix. */
    hash_set<std::string> m_arity_set;

    hash_map<std::string, hash_set<axiom_id_t> >
        m_name_to_axioms, m_lhs_to_axioms, m_rhs_to_axioms,
        m_inc_to_axioms, m_group_to_axioms, m_arity_to_postponement;

    /** Function object to provide distance between predicates. */
    distance_provider_t *m_rm_dist;

    bool m_do_create_local_reachability_matrix;
};


class basic_distance_provider_t : public distance_provider_t
{
public:
    virtual float operator() (const lf::axiom_t&) const
    { return 1.0f; }

    virtual distance_provider_type_e type() const
    { return DISTANCE_PROVIDER_BASIC; }
};


class cost_based_distance_provider_t : public distance_provider_t
{
public:
    virtual float operator()(const lf::axiom_t&) const;
    virtual distance_provider_type_e type() const
    { return DISTANCE_PROVIDER_COST_BASED; }
};


}

}

#include "./kb.inline.h"

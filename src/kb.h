#pragma once

#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <list>
#include <string>
#include <memory>
#include <mutex>
#include <ctime>

#include "./define.h"
#include "./logical_function.h"


namespace phil
{


namespace pg { class proof_graph_t; }


namespace kb
{


typedef unsigned long int argument_set_id_t;

static const axiom_id_t INVALID_AXIOM_ID = -1;
static const argument_set_id_t INVALID_ARGUMENT_SET_ID = 0;


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


enum version_e
{
    KB_VERSION_UNDERSPECIFIED,
    KB_VERSION_1, KB_VERSION_2, KB_VERSION_3, KB_VERSION_4,
    NUM_OF_KB_VERSION_TYPES
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
    static void setup(
        std::string filename, distance_provider_type_e dist_type,
        float max_distance, int thread_num_for_rm);
    static inline float get_max_distance();

    ~knowledge_base_t();

    /** Initializes knowledge base and
     *  prepares for compiling knowledge base. */
    void prepare_compile();

    /** Prepares for reading knowledge base. */
    void prepare_query();

    /** Call this method on end of compiling or reading knowledge base. */
    void finalize();

    axiom_id_t insert_implication(const lf::logical_function_t &f, const std::string &name);
    axiom_id_t insert_inconsistency(const lf::logical_function_t &f, const std::string &name);
    axiom_id_t insert_unification_postponement(const lf::logical_function_t &f, const std::string &name);
    void insert_stop_word_arity(const lf::logical_function_t &f);
    void insert_argument_set(const lf::logical_function_t &f);

    inline lf::axiom_t get_axiom(axiom_id_t id) const;
    inline std::list<axiom_id_t> search_axioms_with_rhs(const std::string &arity) const;
    inline std::list<axiom_id_t> search_axioms_with_lhs(const std::string &arity) const;
    inline std::list<axiom_id_t> search_inconsistencies(const std::string &arity) const;
    hash_set<axiom_id_t> search_axiom_group(axiom_id_t id) const;
    unification_postponement_t get_unification_postponement(const std::string &arity) const;
    argument_set_id_t search_argument_set_id(const std::string &arity, int term_idx) const;

    /** Returns ditance between arity1 and arity2
     *  in a reachable-matrix in the current knowledge-base.
     *  If these arities are not reachable, then return -1. */
    float get_distance(
        const std::string &arity1, const std::string &arity2) const;

    /** Returns distance between arity1 and arity2 with distance-provider. */
    inline float get_distance(const lf::axiom_t &axiom) const;
    inline float get_distance(axiom_id_t id) const;

    inline version_e version() const;
    inline bool is_valid_version() const;

    inline void clear_distance_cache();

private:
    class axioms_database_t
    {
    public:
        axioms_database_t(const std::string &filename);
        ~axioms_database_t();
        void prepare_compile();
        void prepare_query();
        void finalize();
        void put(const std::string &name, const lf::logical_function_t &func);
        lf::axiom_t get(axiom_id_t id) const;
        inline bool is_writable() const;
        inline bool is_readable() const;
        inline int num_axioms() const { return m_num_compiled_axioms; }

    private:
        typedef unsigned long long axiom_pos_t;
        typedef unsigned long axiom_size_t;

        inline std::string get_name_of_unnamed_axiom();

        static std::mutex ms_mutex;
        std::string m_filename;
        std::ofstream *m_fo_idx, *m_fo_dat;
        std::ifstream *m_fi_idx, *m_fi_dat;
        int m_num_compiled_axioms, m_num_unnamed_axioms;
        axiom_pos_t m_writing_pos;
    };

    /** A class of reachable-matrix for all predicate pairs. */
    class reachable_matrix_t
    {
    public:
        reachable_matrix_t(const std::string &filename);
        ~reachable_matrix_t();
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
        static std::mutex ms_mutex;
        std::string   m_filename;
        std::ofstream *m_fout;
        std::ifstream *m_fin;
        hash_map<size_t, pos_t> m_map_idx_to_pos;
    };

    enum kb_state_e { STATE_NULL, STATE_COMPILE, STATE_QUERY };

    knowledge_base_t(
        const std::string &filename, distance_provider_type_e dist);

    void write_config() const;
    void read_config();

    void _insert_cdb(
        const hash_map<std::string, hash_set<axiom_id_t> > &ids, cdb_data_t *dat);
    void insert_arity(const std::string &arity);

    /** Outputs m_group_to_axioms to m_cdb_axiom_group. */
    void insert_axiom_group_to_cdb();
    void insert_argument_set_to_cdb();

    /** Creates reachable matrix.
     *  This is a sub-routine of finalize. */
    void create_reachable_matrix();
    
    void _create_reachable_matrix_direct(
        const hash_set<std::string> &arities,
        const hash_set<size_t> &ignored,
        hash_map<size_t, hash_map<size_t, float> > *out_lhs,
        hash_map<size_t, hash_map<size_t, float> > *out_rhs,
        std::set<std::pair<size_t, size_t> > *out_para);
    void _create_reachable_matrix_indirect(
        size_t key,
        const hash_map<size_t, hash_map<size_t, float> > &base_lhs,
        const hash_map<size_t, hash_map<size_t, float> > &base_rhs,
        const std::set<std::pair<size_t, size_t> > &base_para,
        hash_map<size_t, float> *out) const;

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

    static std::unique_ptr<knowledge_base_t, deleter> ms_instance;
    static std::string ms_filename;
    static distance_provider_type_e ms_distance_provider_type;
    static float ms_max_distance;
    static int ms_thread_num_for_rm;
    static std::mutex ms_mutex_for_cache;
    static std::mutex ms_mutex_for_rm;

    kb_state_e m_state;
    std::string m_filename;
    version_e m_version;

    cdb_data_t m_cdb_name, m_cdb_rhs, m_cdb_lhs;
    cdb_data_t m_cdb_inc_pred, m_cdb_axiom_group, m_cdb_uni_pp, m_cdb_arg_set;
    cdb_data_t m_cdb_rm_idx;
    axioms_database_t m_axioms;
    reachable_matrix_t m_rm;

    hash_map<size_t, hash_map<size_t, float> > m_partial_reachable_matrix;

    /** All arities in this knowledge-base.
     *  This variable is used on constructing a reachable-matrix. */
    hash_set<std::string> m_arity_set;

    /** A set of arities of stop-words.
     *  These arities are ignored in constructing a reachable-matrix. */
    hash_set<std::string> m_stop_words;

    std::list<hash_set<std::string> > m_argument_sets;

    hash_map<std::string, hash_set<axiom_id_t> >
        m_name_to_axioms, m_lhs_to_axioms, m_rhs_to_axioms,
        m_inc_to_axioms, m_group_to_axioms, m_arity_to_postponement;

    /** Function object to provide distance between predicates. */
    distance_provider_t *m_rm_dist;

    bool m_do_create_local_reachability_matrix;
    
    mutable hash_map<size_t, hash_map<size_t, float> > m_cache_distance;
};


class basic_distance_provider_t : public distance_provider_t
{
public:
    virtual float operator() (const lf::axiom_t&) const;
    virtual distance_provider_type_e type() const { return DISTANCE_PROVIDER_BASIC; }
};


class cost_based_distance_provider_t : public distance_provider_t
{
public:
    virtual float operator()(const lf::axiom_t&) const;
    virtual distance_provider_type_e type() const { return DISTANCE_PROVIDER_COST_BASED; }
};


}

}

#include "./kb.inline.h"

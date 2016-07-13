#pragma once

#include <iostream>
#include <fstream>
#include <tuple>
#include <map>
#include <set>
#include <list>
#include <string>
#include <memory>
#include <mutex>
#include <ctime>

#include "./define.h"
#include "./sexp.h"
#include "./logical_function.h"


namespace phil
{


namespace pg { class proof_graph_t; }


namespace kb
{

typedef long relation_flags_t;


enum variable_unifiability_type_e : char
{
    UNI_STRONGLY_LIMITED, /// Is expressed as '*'.
    UNI_WEAKLY_LIMITED,   /// Is expressed as '+'.
    UNI_UNLIMITED,        /// Is expressed as '.'.
};


enum relation_e : long
{
    REL_NONE         = 0x00000000,
    REL_IRREFLEXIVE  = 0x00000001,
    REL_SYMMETRIC    = 0x00000010,
    REL_ASYMMETRIC   = 0x00000100,
    REL_TRANSITIVE   = 0x00001000,
    REL_RIGHT_UNIQUE = 0x00010000
};


enum version_e
{
    KB_VERSION_UNSPECIFIED,
    KB_VERSION_1, KB_VERSION_2, KB_VERSION_3, KB_VERSION_4, KB_VERSION_5,
    KB_VERSION_6, KB_VERSION_7, KB_VERSION_8, KB_VERSION_9, KB_VERSION_10,
    KB_VERSION_11, KB_VERSION_12,
    NUM_OF_KB_VERSION_TYPES
};


/** A class for keys to look up KB.
* The first element is a predicate list.
* The second is argument pairs which are hard-term.
*/
class conjunction_pattern_t
    : public std::pair<std::vector<predicate_id_t>, std::list<std::pair<term_pos_t, term_pos_t>>>
{
public:
    std::vector<predicate_id_t>& predicates() { return this->first; }
    const std::vector<predicate_id_t>& predicates() const { return this->first; }

    std::list<std::pair<term_pos_t, term_pos_t>>& hardterms() { return this->second; }
    const std::list<std::pair<term_pos_t, term_pos_t>>& hardterms() const { return this->second; }
};


/** A virtual class to define distance between predicates
 *  on creation of reachable-matrix. */
class distance_function_t
{
public:
    virtual ~distance_function_t() {}
    virtual float operator() (const lf::axiom_t &ax) const = 0;

    virtual void read(std::ifstream *fi) = 0;
    virtual void write(std::ofstream *fo) const = 0;

    virtual std::string repr() const = 0;
};


class functional_predicate_configuration_t
{
public:
    functional_predicate_configuration_t();
    functional_predicate_configuration_t(predicate_id_t arity, relation_flags_t rel);
    functional_predicate_configuration_t(const lf::logical_function_t &f);
    functional_predicate_configuration_t(std::ifstream *fi);

    void write(std::ofstream *fo) const;

    predicate_id_t predicate_id() const { return m_pid; }
    bool do_postpone(const pg::proof_graph_t*, index_t n1, index_t n2) const;

    bool is_irreflexive() const { return m_rel & REL_IRREFLEXIVE; }
    bool is_symmetric() const { return m_rel & REL_SYMMETRIC; }
    bool is_asymmetric() const { return m_rel & REL_ASYMMETRIC; }
    bool is_transitive() const { return m_rel & REL_TRANSITIVE; }
    bool is_right_unique() const { return m_rel & REL_RIGHT_UNIQUE; }

    term_idx_t governor() const { return (m_unifiability.size() == 3) ? 1 : 0; }
    term_idx_t dependent() const { return (m_unifiability.size() == 3) ? 2 : 1; }

    inline bool is_good() const;
    string_t repr() const;

private:
    void assign_unifiability(relation_flags_t flags, arity_t n);

    predicate_id_t m_pid;
    std::vector<variable_unifiability_type_e> m_unifiability;
    relation_flags_t m_rel;
};


/** A class of knowledge-base. */
class knowledge_base_t
{
public:
    static void initialize(std::string filename, const phillip_main_t *ph);
    static knowledge_base_t* instance();

    ~knowledge_base_t();

    void prepare_compile(); /// Prepares for compiling knowledge base.
    void prepare_query();   /// Prepares for reading knowledge base.
    void finalize();        /// Is called on the end of compiling or reading.

    void set_distance_provider(const std::string &key, const phillip_main_t *ph = NULL);

    /** Returns ditance between arity1 and arity2
     *  in a reachable-matrix in the current knowledge-base.
     *  If these arities are not reachable, then return -1. */
    float get_distance(const std::string &a1, const std::string &a2) const;
    float get_distance(predicate_id_t a1, predicate_id_t a2) const;

    /** Returns distance between arity1 and arity2 with distance-provider. */
    inline float get_distance(const lf::axiom_t &axiom) const;
    inline float get_distance(axiom_id_t id) const;

    inline version_e version() const     { return m_version; }
    inline bool is_valid_version() const { return m_version == KB_VERSION_12; }
    inline bool is_writable() const      { return m_state == STATE_COMPILE; }
    inline bool is_readable() const      { return m_state == STATE_QUERY; }
    inline bool can_deduce() const { return m_config_for_compile.can_deduction; }
    inline const std::string& filename() const { return m_filename; }
    inline float get_max_distance() const;

    inline void clear_distance_cache();

    /** A class of database of axioms. */
    class axioms_database_t
    {
        friend knowledge_base_t;
    private:
        axioms_database_t(const std::string &filename);

        void prepare_compile();
        void prepare_query();
        void finalize();

    public:
        ~axioms_database_t();

        axiom_id_t add(const lf::logical_function_t &func, const string_t &name = "");
        lf::axiom_t get(axiom_id_t id) const;

        inline std::list<axiom_id_t> gets_by_rhs(predicate_id_t rhs) const;
        inline std::list<axiom_id_t> gets_by_rhs(const predicate_with_arity_t &rhs) const;
        inline std::list<axiom_id_t> gets_by_lhs(predicate_id_t lhs) const;
        inline std::list<axiom_id_t> gets_by_lhs(const predicate_with_arity_t &lhs) const;

        std::list<conjunction_pattern_t> patterns(predicate_id_t arity) const;
        std::list<std::pair<axiom_id_t, bool>> gets_by_pattern(const conjunction_pattern_t &query) const;

        hash_set<axiom_id_t> gets_in_same_group_as(axiom_id_t id) const;

        inline size_t size() const   { return static_cast<size_t>(m_num_compiled_axioms); }
        inline bool is_writable() const { return (m_fo_idx != NULL) and(m_fo_dat != NULL); }
        inline bool is_readable() const { return (m_fi_idx != NULL) and (m_fi_dat != NULL); }
        inline bool empty() const       { return size() == 0; }

    private:
        typedef unsigned long long axiom_pos_t;
        typedef unsigned long axiom_size_t; /// The type of binary size of axiom.
        typedef string_t group_name_t;

        inline std::string get_name_of_unnamed_axiom();
        std::list<axiom_id_t> gets_from_cdb(const char *key, size_t key_size, const util::cdb_data_t *dat) const;
        void build_conjunct_predicates_map(); /// Sub routine in finalize.

        static std::mutex ms_mutex;

        std::string m_filename;
        std::unique_ptr<std::ofstream> m_fo_idx, m_fo_dat;
        std::unique_ptr<std::ifstream> m_fi_idx, m_fi_dat;
        int m_num_compiled_axioms, m_num_unnamed_axioms;
        axiom_pos_t m_writing_pos;

        util::cdb_data_t m_cdb_rhs, m_cdb_lhs;
        util::cdb_data_t m_cdb_arity_patterns, m_cdb_pattern_to_ids;

        struct
        {
            hash_map<axiom_id_t, std::list<hash_set<axiom_id_t>*> > ax2gr;
            std::list<hash_set<axiom_id_t> > groups;
        } m_groups;

        struct
        {
            hash_map<group_name_t, hash_set<axiom_id_t> > gr2ax;
            hash_map<predicate_id_t, hash_set<axiom_id_t> > lhs2ax, rhs2ax;
            hash_map<predicate_id_t, std::set<conjunction_pattern_t>> pred2pats;
            std::map<conjunction_pattern_t, std::set<std::pair<axiom_id_t, is_backward_t>>> pat2ax;
        } m_tmp; /// Temporal variables for compiling.
    } axioms;

    /** A class of the list of predicates. */
    class predicate_database_t
    {
        friend knowledge_base_t;
    private:
        predicate_database_t(const std::string &filename);

        void clear();
        void load();
        void write() const;

    public:
        predicate_id_t add(const predicate_with_arity_t&);
        predicate_id_t add(const literal_t&);

        void define_functional_predicate(const lf::logical_function_t&);
        void define_functional_predicate(const functional_predicate_configuration_t&);

        void define_mutual_exclusion(const lf::logical_function_t &f);
        void define_mutual_exclusion(const literal_t &l1, const literal_t &l2);

        inline const std::vector<predicate_with_arity_t>& arities() const;
        inline predicate_id_t pred2id(const predicate_with_arity_t&) const;
        inline const predicate_with_arity_t& id2pred(predicate_id_t) const;

        inline const hash_map<predicate_id_t, functional_predicate_configuration_t>& functional_predicates() const;
        inline const functional_predicate_configuration_t* find_functional_predicate(predicate_id_t) const;
        inline const functional_predicate_configuration_t* find_functional_predicate(const predicate_with_arity_t&) const;
        inline bool is_functional(predicate_id_t) const;
        inline bool is_functional(const predicate_with_arity_t&) const;

        inline const std::list<std::pair<term_idx_t, term_idx_t> >*
            find_inconsistent_terms(predicate_id_t, predicate_id_t) const;

    private:
        std::string m_filename;

        std::vector<predicate_with_arity_t> m_arities;
        hash_map<predicate_with_arity_t, predicate_id_t> m_arity2id;

        hash_map<predicate_id_t, functional_predicate_configuration_t> m_functional_predicates;
        hash_map<predicate_id_t, hash_map<predicate_id_t,
            std::list<std::pair<term_idx_t, term_idx_t> > > > m_mutual_exclusions;
    } predicates;

    /** A class of reachable-matrix for all predicate pairs. */
    class reachable_matrix_t
    {
        friend knowledge_base_t;
    private:
        reachable_matrix_t(const std::string &filename);

        void prepare_compile();
        void prepare_query();
        void finalize();

    public:
        void put(size_t idx1, const hash_map<size_t, float> &dist);
        float get(size_t idx1, size_t idx2) const;
        hash_set<float> get(size_t idx) const;

        inline bool is_writable() const { return static_cast<bool>(m_fout); }
        inline bool is_readable() const { return static_cast<bool>(m_fin); }

    private:
        typedef unsigned long long pos_t;
        static std::mutex ms_mutex;
        std::string  m_filename;
        std::unique_ptr<std::ofstream> m_fout;
        std::unique_ptr<std::ifstream> m_fin;
        hash_map<size_t, pos_t> m_map_idx_to_pos;
    } heuristics;

private:
    enum kb_state_e { STATE_NULL, STATE_COMPILE, STATE_QUERY };

    knowledge_base_t(const std::string &filename, const phillip_main_t *ph);

    void write_config() const;
    void read_config();

    void create_reachable_matrix();
    
    void _create_reachable_matrix_direct(
        const hash_set<predicate_id_t> &ignored,
        hash_map<predicate_id_t, hash_map<predicate_id_t, float> > *out_lhs,
        hash_map<predicate_id_t, hash_map<predicate_id_t, float> > *out_rhs,
        std::set<std::pair<predicate_id_t, predicate_id_t> > *out_para);
    void _create_reachable_matrix_indirect(
        predicate_id_t target,
        const hash_map<predicate_id_t, hash_map<predicate_id_t, float> > &base_lhs,
        const hash_map<predicate_id_t, hash_map<predicate_id_t, float> > &base_rhs,
        const std::set<std::pair<predicate_id_t, predicate_id_t> > &base_para,
        hash_map<predicate_id_t, float> *out) const;

    void extend_inconsistency();

    static std::unique_ptr<knowledge_base_t, util::deleter_t<knowledge_base_t> > ms_instance;
    static std::mutex ms_mutex_for_cache;
    static std::mutex ms_mutex_for_rm;

    kb_state_e m_state;
    std::string m_filename;
    version_e m_version;

    struct
    {
        float max_distance;
        int thread_num;
        bool do_disable_stop_word;
        bool can_deduction;
        bool do_print_heuristics;
    } m_config_for_compile;

    /** Function object to provide distance between predicates. */
    struct
    {
        distance_function_t *instance;
        std::string key;
    } m_distance_provider;

    mutable hash_map<size_t, hash_map<size_t, float> > m_cache_distance;
};


/** The namespace for super-classes of distance_function_t. */
namespace dist
{


class null_distance_provider_t : public distance_function_t
{
public:
    struct generator_t : public component_generator_t<distance_function_t>
    {
        virtual distance_function_t* operator()(const phillip_main_t*) const override
        { return new null_distance_provider_t(); }
    };

    virtual void read(std::ifstream *fi) {}
    virtual void write(std::ofstream *fo) const {}

    virtual float operator() (const lf::axiom_t&) const;
    virtual std::string repr() const { return "null"; };
};

class basic_distance_provider_t : public distance_function_t
{
public:
    struct generator_t : public component_generator_t<distance_function_t>
    {
        virtual distance_function_t* operator()(const phillip_main_t*) const override
            { return new basic_distance_provider_t(); }
    };

    virtual void read(std::ifstream *fi) {}
    virtual void write(std::ofstream *fo) const {}

    virtual float operator() (const lf::axiom_t&) const;
    virtual std::string repr() const { return "basic"; };
};


class cost_based_distance_provider_t : public distance_function_t
{
public:
    struct generator_t : public component_generator_t<distance_function_t>
    {
        virtual distance_function_t* operator()(const phillip_main_t*) const override
            { return new cost_based_distance_provider_t(); }
    };

    virtual void read(std::ifstream *fi) {}
    virtual void write(std::ofstream *fo) const {}

    virtual float operator()(const lf::axiom_t&) const;
    virtual std::string repr() const { return "cost-based"; }
};


class sum_of_left_hand_side_distance_provider_t : public distance_function_t
{
public:
    struct generator_t : public component_generator_t<distance_function_t>
    {
        virtual distance_function_t* operator()(const phillip_main_t*) const override;
    };

    sum_of_left_hand_side_distance_provider_t(float d) : m_default_distance(d) {}
    
    virtual void read(std::ifstream *fi);
    virtual void write(std::ofstream *fo) const;

    virtual float operator()(const lf::axiom_t&) const;
    virtual std::string repr() const { return "sum_of_left-hand-side"; }

private:
    float m_default_distance;
};

}


void pattern_to_binary(const conjunction_pattern_t &q, std::vector<char> *bin);
size_t binary_to_pattern(const char *bin, conjunction_pattern_t *out);

inline knowledge_base_t* kb() { return knowledge_base_t::instance(); }

} // END OF kb

} // END OF phil


#include "./kb.inline.h"

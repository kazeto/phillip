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

#include "./util.h"
#include "./fol.h"

namespace dav
{


namespace kb
{



enum version_e
{
    KB_VERSION_UNSPECIFIED,
    KB_VERSION_1, KB_VERSION_2, KB_VERSION_3, KB_VERSION_4, KB_VERSION_5,
    KB_VERSION_6, KB_VERSION_7, KB_VERSION_8, KB_VERSION_9, KB_VERSION_10,
    KB_VERSION_11, KB_VERSION_12,
    NUM_OF_KB_VERSION_TYPES
};


/** A virtual class to define distance between predicates
 *  on creation of reachable-matrix. */
class distance_function_t
{
public:
    virtual ~distance_function_t() {}

	virtual double operator()(const rule_t&) const = 0;

    virtual std::string repr() const = 0;
};


struct configuration_t
{
	configuration_t();

	filepath_t path;
	double max_distance;
	int thread_num;
	string_t dp_key; /// Key of distance-provider
};


/** A class of knowledge-base. */
class knowledge_base_t
{
public:
    static void initialize(const configuration_t &conf);
    static knowledge_base_t* instance();

    ~knowledge_base_t();

    void prepare_compile(); /// Prepares for compiling knowledge base.
    void prepare_query();   /// Prepares for reading knowledge base.
    void finalize();        /// Is called on the end of compiling or reading.

    void set_distance_provider(const string_t &key);

    /** Returns ditance between arity1 and arity2
     *  in a reachable-matrix in the current knowledge-base.
     *  If these arities are not reachable, then return -1. */
    float get_distance(const std::string &a1, const std::string &a2) const;
    float get_distance(predicate_id_t a1, predicate_id_t a2) const;

    /** Returns distance between arity1 and arity2 with distance-provider. */
    inline float get_distance(axiom_id_t id) const;

    version_e version() const     { return m_version; }
    bool is_valid_version() const { return m_version == KB_VERSION_12; }
    bool is_writable() const      { return m_state == STATE_COMPILE; }
    bool is_readable() const      { return m_state == STATE_QUERY; }
    const std::string& filename() const { return m_config.path; }
	float get_max_distance() const { return m_config.max_distance; }

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

        axiom_id_t add(const rule_t &r);
        rule_t get(axiom_id_t id) const;

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
        std::list<axiom_id_t> gets_from_cdb(
			const char *key, size_t key_size, const cdb_data_t *dat) const;

        static std::mutex ms_mutex;

        std::string m_filename;
        std::unique_ptr<std::ofstream> m_fo_idx, m_fo_dat;
        std::unique_ptr<std::ifstream> m_fi_idx, m_fi_dat;
        int m_num_compiled_axioms, m_num_unnamed_axioms;
        axiom_pos_t m_writing_pos;

        cdb_data_t m_cdb_rhs, m_cdb_lhs;
        cdb_data_t m_cdb_arity_patterns, m_cdb_pattern_to_ids;

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

    knowledge_base_t(const configuration_t &conf);

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

    static std::unique_ptr<knowledge_base_t, deleter_t<knowledge_base_t>> ms_instance;
    static std::mutex ms_mutex_for_cache;
    static std::mutex ms_mutex_for_rm;

    kb_state_e m_state;
    version_e m_version;

    configuration_t m_config;

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


class null_distance_function_t : public distance_function_t
{
public:
    struct generator_t : public component_generator_t<distance_function_t>
    {
        virtual distance_function_t* operator()() const override
        { return new null_distance_function_t(); }
    };

	virtual double operator() (const rule_t&) const override { return 0.0; }
    virtual std::string repr() const override { return "null"; };
};


class basic_distance_function_t : public distance_function_t
{
public:
    struct generator_t : public component_generator_t<distance_function_t>
    {
        virtual distance_function_t* operator()() const override
            { return new basic_distance_function_t(); }
    };

	virtual double operator() (const rule_t&) const override { return 1.0; }
    virtual std::string repr() const override { return "basic"; };
};

} // end of dist


inline knowledge_base_t* kb() { return knowledge_base_t::instance(); }

} // end of kb

} // end of dav


#include "./kb.inline.h"

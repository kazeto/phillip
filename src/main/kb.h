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


/** A class to strage all patterns of conjunctions found in KB. */
class conjunction_library_t : public cdb_data_t
{
public:
	conjunction_library_t(const filepath_t &path);

	void prepare_compile();
	void prepare_query();
	void finalize();

	void insert(const rule_t&);
	std::list<conjunction_t::feature_t> get(predicate_id_t) const;

private:
	std::unordered_map<predicate_id_t, std::set<conjunction_t::feature_t>> m_features;
};


/** Wrapper class of CDB for map of rule-set. */
template <typename T> class rules_cdb_t : public cdb_data_t
{
public:
	rules_cdb_t(const filepath_t &path) : cdb_data_t(path) {}

	void prepare_compile();
	void finalize();

	std::list<rule_id_t> gets(const T &key) const;
	void insert(const T&, rule_id_t);

private:
	std::map<T, std::unordered_set<rule_id_t>> m_rids;
};


/** A class of database of axioms. */
class rule_library_t
{
public:
	rule_library_t(const filepath_t &filename);
	~rule_library_t();

	void prepare_compile();
	void prepare_query();
	void finalize();

	rule_id_t add(rule_t &r);
	rule_t get(rule_id_t id) const;

	inline size_t size() const { return m_num_rules; }
	inline bool is_writable() const { return (m_fo_idx != NULL) and (m_fo_dat != NULL); }
	inline bool is_readable() const { return (m_fi_idx != NULL) and (m_fi_dat != NULL); }
	inline bool empty() const { return size() == 0; }

private:
	typedef unsigned long long pos_t;
	typedef unsigned long rule_size_t; /// The type of binary size of axiom.

	string_t get_name_of_unnamed_axiom();

	static std::mutex ms_mutex;

	filepath_t m_filename;
	std::unique_ptr<std::ofstream> m_fo_idx, m_fo_dat;
	std::unique_ptr<std::ifstream> m_fi_idx, m_fi_dat;
	size_t m_num_rules;
	size_t m_num_unnamed_rules;
	pos_t m_writing_pos;
};



/** A virtual class to define distance between predicates.
*  An instance of this class will be used on creation of reachable-matrix. */
class distance_function_t
{
public:
	virtual ~distance_function_t() {}

	virtual double operator()(const rule_t&) const = 0;
	virtual std::string repr() const = 0;
};


/** A class of reachability-matrix for all predicate pairs. */
class reachability_matrix_t
{
public:
	static void initialize(const filepath_t &path);
	static reachability_matrix_t* instance();

	void load(); // TODO
	void write(); // TODO

	void construct(); // TODO

	void prepare_compile();
	void prepare_query();
	void finalize();

	float get(size_t idx1, size_t idx2) const;
	hash_set<float> get(size_t idx) const;

	bool is_writable() const { return static_cast<bool>(m_fout); }
	bool is_readable() const { return static_cast<bool>(m_fin); }

private:
	typedef unsigned long long pos_t;

	reachability_matrix_t(const filepath_t &path);
	void put(size_t idx1, const hash_map<size_t, float> &dist);

	static std::unique_ptr<reachability_matrix_t> ms_instance;
	static std::mutex ms_mutex;

	filepath_t m_path;
	std::unique_ptr<std::ofstream> m_fout;
	std::unique_ptr<std::ifstream> m_fin;
	hash_map<size_t, pos_t> m_map_idx_to_pos;
};



/** A struct for configuration of KB. */
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

    rule_library_t rules;
	rules_cdb_t<predicate_id_t> lhs2rids;
	rules_cdb_t<predicate_id_t> rhs2rids;
	rules_cdb_t<rule_class_t> class2rids;
	rules_cdb_t<conjunction_t::feature_t> feats2rids;

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

    static std::unique_ptr<knowledge_base_t, deleter_t<knowledge_base_t>> ms_instance;
    static std::mutex ms_mutex_for_cache;
    static std::mutex ms_mutex_for_rm;

    kb_state_e m_state;
    version_e m_version;

    configuration_t m_config;
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

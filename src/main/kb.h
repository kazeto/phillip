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
    KB_VERSION_1,
    NUM_OF_KB_VERSION_TYPES
};


/** A class to strage all patterns of conjunctions found in KB. */
class conjunction_library_t : public cdb_data_t
{
public:
	struct elem_t
	{
		conjunction_t::feature_t feature;
		is_backward_t is_backward;
	};

	conjunction_library_t(const filepath_t &path);
	~conjunction_library_t();

	virtual void prepare_compile() override;
	virtual void finalize() override;

	void insert(const rule_t&);
	std::list<elem_t> get(predicate_id_t) const;

private:
	std::unordered_map<
		predicate_id_t,
		std::set<std::pair<conjunction_t::feature_t, is_backward_t>>> m_features;
};


/** A class of CDB for map from  */
class feature_to_rules_cdb_t : public cdb_data_t
{
public:
	feature_to_rules_cdb_t(const filepath_t &path) : cdb_data_t(path) {}

	virtual void prepare_compile() override;
	virtual void finalize() override;

	std::list<rule_id_t> gets(const conjunction_t::feature_t&, is_backward_t) const;
	void insert(const conjunction_t&, is_backward_t, rule_id_t);

private:
	std::map<
		std::pair<conjunction_t::feature_t, is_backward_t>,
		std::unordered_set<rule_id_t>> m_feat2rids;
};


/** Wrapper class of CDB for map of rule-set. */
template <typename T> class rules_cdb_t : public cdb_data_t
{
public:
	rules_cdb_t(const filepath_t &path) : cdb_data_t(path) {}

	virtual void prepare_compile() override;
	virtual void finalize() override;

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


/** A class of knowledge-base. */
class knowledge_base_t
{
public:
    static void initialize(const filepath_t&);
    static knowledge_base_t* instance();

    ~knowledge_base_t();

    void prepare_compile(); /// Prepares for compiling knowledge base.
    void prepare_query();   /// Prepares for reading knowledge base.
    void finalize();        /// Is called on the end of compiling or reading.

    version_e version() const     { return m_version; }
    bool is_valid_version() const { return m_version == KB_VERSION_1; }
    bool is_writable() const      { return m_state == STATE_COMPILE; }
    bool is_readable() const      { return m_state == STATE_QUERY; }
    const filepath_t& filepath() const { return m_path; }

    rule_library_t rules;
	conjunction_library_t features;
	feature_to_rules_cdb_t feat2rids;
	rules_cdb_t<predicate_id_t> lhs2rids;
	rules_cdb_t<predicate_id_t> rhs2rids;
	rules_cdb_t<rule_class_t> class2rids;

private:
    enum kb_state_e { STATE_NULL, STATE_COMPILE, STATE_QUERY };

    knowledge_base_t(const filepath_t&);
	void write_spec(const filepath_t&) const;

    static std::unique_ptr<knowledge_base_t, deleter_t<knowledge_base_t>> ms_instance;

    kb_state_e m_state;
    version_e m_version;
	filepath_t m_path;
};

inline knowledge_base_t* kb() { return knowledge_base_t::instance(); }


} // end of kb

} // end of dav


#include "./kb.inline.h"

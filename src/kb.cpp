/* -*- coding: utf-8 -*- */

#include <iomanip>
#include <cassert>
#include <cstring>
#include <climits>
#include <algorithm>
#include <thread>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "./kb.h"
#include "./phillip.h"
#include "./sol/ilp_solver.h"
#include "./binary.h"


namespace phil
{

namespace kb
{


unification_postponement_t::unification_postponement_t(
    arity_id_t arity, const std::vector<char> &args,
    small_size_t num_for_partial_indispensability)
    : m_arity(arity), m_args(args),
      m_num_for_partial_indispensability(num_for_partial_indispensability)
{
    int n = std::count(
        m_args.begin(), m_args.end(), UNI_PP_INDISPENSABLE_PARTIALLY);

    if (m_num_for_partial_indispensability < 0)
        m_num_for_partial_indispensability = 0;
    if (m_num_for_partial_indispensability > n)
        m_num_for_partial_indispensability = n;
}


unification_postponement_t::unification_postponement_t(std::ifstream *fi)
{
    small_size_t num_args;

    fi->read((char*)&m_arity, sizeof(arity_id_t));
    fi->read((char*)&num_args, sizeof(small_size_t));

    m_args.assign(num_args, 0);
    for (unsigned char i = 0; i < num_args; ++i)
        fi->read(&m_args[i], sizeof(char));

    fi->read((char*)&m_num_for_partial_indispensability, sizeof(small_size_t));
}


void unification_postponement_t::write(std::ofstream *fo) const
{
    small_size_t num_args = m_args.size();

    fo->write((char*)&m_arity, sizeof(arity_id_t));
    fo->write((char*)&num_args, sizeof(small_size_t));

    for (auto arg : m_args)
        fo->write(&arg, sizeof(char));

    fo->write((char*)&m_num_for_partial_indispensability, sizeof(small_size_t));
}


bool unification_postponement_t::do_postpone(
    const pg::proof_graph_t *graph, index_t n1, index_t n2) const
{
#ifdef DISABLE_UNIPP
    return false;
#else
    const literal_t &l1 = graph->node(n1).literal();
    const literal_t &l2 = graph->node(n2).literal();
    int num(0);

    assert(
        l1.terms.size() == m_args.size() and
        l2.terms.size() == m_args.size());

    for (size_t i = 0; i < m_args.size(); ++i)
    {
        unification_postpone_argument_type_e arg =
            static_cast<unification_postpone_argument_type_e>(m_args.at(i));

        if (arg == UNI_PP_DISPENSABLE) continue;
        
        bool can_equal(l1.terms.at(i) == l2.terms.at(i));
        if (not can_equal)
            can_equal = (graph->find_sub_node(l1.terms.at(i), l2.terms.at(i)) >= 0);

        if (arg == UNI_PP_INDISPENSABLE and not can_equal)
            return true;
        if (arg == UNI_PP_INDISPENSABLE_PARTIALLY and can_equal)
            ++num;
    }

    return (num < m_num_for_partial_indispensability);
#endif
}


const int BUFFER_SIZE = 512 * 512;
std::unique_ptr<knowledge_base_t, util::deleter_t<knowledge_base_t> > knowledge_base_t::ms_instance;
std::mutex knowledge_base_t::ms_mutex_for_cache;
std::mutex knowledge_base_t::ms_mutex_for_rm;


knowledge_base_t* knowledge_base_t::instance()
{
    if (not ms_instance)
        throw phillip_exception_t("An instance of knowledge-base has not been initialized.");

    return ms_instance.get();
}


void knowledge_base_t::initialize(std::string filename, const phillip_main_t *ph)
{
    if (ms_instance != NULL)
        ms_instance.reset(NULL);
        
    util::mkdir(util::get_directory_name(filename));
    ms_instance.reset(new knowledge_base_t(filename, ph));
}


knowledge_base_t::knowledge_base_t(const std::string &filename, const phillip_main_t *ph)
    : m_state(STATE_NULL),
      m_filename(filename), m_version(KB_VERSION_1), 
      m_cdb_rhs(filename + ".rhs.cdb"),
      m_cdb_lhs(filename + ".lhs.cdb"),
      m_cdb_arg_set(filename + ".args.cdb"),
      m_cdb_arity_patterns(filename + ".pattern.cdb"),
      m_cdb_pattern_to_ids(filename + ".search.cdb"),
      m_axioms(filename),
      m_arity_db(filename + ".arity.dat"),
      m_rm(filename + ".rm.dat")
{
    m_distance_provider = { NULL, "" };
    m_category_table = { NULL, "" };

    m_config_for_compile.max_distance = ph->param_float("kb_max_distance", -1.0);
    m_config_for_compile.thread_num = ph->param_int("kb_thread_num", 1);
    m_config_for_compile.do_disable_stop_word = ph->flag("disable_stop_word");
    m_config_for_compile.can_deduction = ph->flag("enable_deduction");

    if (m_config_for_compile.thread_num < 1)
        m_config_for_compile.thread_num = 1;

    std::string dist_key = ph->param("distance_provider");
    std::string tab_key = ph->param("category_table");

    set_distance_provider(dist_key.empty() ? "basic" : dist_key, ph);
    set_category_table(tab_key.empty() ? "null" : tab_key, ph);
}


knowledge_base_t::~knowledge_base_t()
{
    finalize();

    if (m_distance_provider.instance != NULL)
        delete m_distance_provider.instance;
}


void knowledge_base_t::prepare_compile()
{
    if (m_distance_provider.instance == NULL)
        throw phillip_exception_t(
        "Preparing KB had failed, "
        "because distance provider has not been set.");

    if (m_category_table.instance == NULL)
        throw phillip_exception_t(
        "Preparing KB had failed, "
        "because category table has not been set.");

    if (m_state == STATE_QUERY)
        finalize();

    if (m_state == STATE_NULL)
    {
        m_axioms.prepare_compile();
        m_cdb_rhs.prepare_compile();
        m_cdb_lhs.prepare_compile();
        m_cdb_arg_set.prepare_compile();
        m_cdb_arity_patterns.prepare_compile();
        m_cdb_pattern_to_ids.prepare_compile();
        m_category_table.instance->prepare_compile(this);

        m_state = STATE_COMPILE;
    }
}


void knowledge_base_t::prepare_query()
{
    if (m_distance_provider.instance == NULL)
        throw phillip_exception_t(
        "Preparing KB had failed, "
        "because distance provider has not been set.");

    if (m_category_table.instance == NULL)
        throw phillip_exception_t(
        "Preparing KB had failed, "
        "because category table has not been set.");

    if (m_state == STATE_COMPILE)
        finalize();

    if (m_state == STATE_NULL)
    {
        read_config();
        read_axiom_group();
        m_arity_db.read();

        m_axioms.prepare_query();
        m_cdb_rhs.prepare_query();
        m_cdb_lhs.prepare_query();
        m_cdb_arg_set.prepare_query();
        m_cdb_arity_patterns.prepare_query();
        m_cdb_pattern_to_ids.prepare_query();
        m_rm.prepare_query();
        m_category_table.instance->prepare_query(this);

        m_state = STATE_QUERY;
    }
}


void knowledge_base_t::finalize()
{
    if (m_state == STATE_NULL) return;

    kb_state_e state = m_state;
    m_state = STATE_NULL;

    if (state == STATE_COMPILE)
    {
        auto insert_cdb = [](
            const hash_map<arity_id_t, hash_set<axiom_id_t> > &ids,
            util::cdb_data_t *dat)
        {
            IF_VERBOSE_1("starts writing " + dat->filename() + "...");

            for (auto it = ids.begin(); it != ids.end(); ++it)
            {
                size_t read_size = sizeof(size_t)+sizeof(axiom_id_t)* it->second.size();
                char *buffer = new char[read_size];

                int size = util::to_binary<size_t>(it->second.size(), buffer);
                for (auto id = it->second.begin(); id != it->second.end(); ++id)
                    size += util::to_binary<axiom_id_t>(*id, buffer + size);

                assert(read_size == size);
                dat->put((char*)&it->first, sizeof(arity_id_t), buffer, size);
                delete[] buffer;
            }

            IF_VERBOSE_1("completed writing " + dat->filename() + ".");
        };

        extend_inconsistency();

        insert_cdb(m_rhs_to_axioms, &m_cdb_rhs);
        insert_cdb(m_lhs_to_axioms, &m_cdb_lhs);
        write_axiom_group();
        insert_argument_set_to_cdb();

        m_rhs_to_axioms.clear();
        m_lhs_to_axioms.clear();
        m_group_to_axioms.clear();
        m_argument_sets.clear();

        set_stop_words();
        create_query_map();
        create_reachable_matrix();
        write_config();
        m_arity_db.write();

        if (phillip_main_t::verbose() == FULL_VERBOSE)
        {
            std::cerr << "Reachability Matrix:" << std::endl;
            m_rm.prepare_query();

            std::cerr << std::setw(30) << std::right << "" << " | ";
            for (auto arity : m_arity_db.arities())
                std::cerr << arity << " | ";
            std::cerr << std::endl;

            for (auto a1 : m_arity_db.arities())
            {
                arity_id_t idx1 = search_arity_id(a1);
                std::cerr << std::setw(30) << std::right << a1 << " | ";

                for (auto a2 : m_arity_db.arities())
                {
                    arity_id_t idx2 = search_arity_id(a2);
                    float dist = m_rm.get(idx1, idx2);
                    std::cerr << std::setw(a2.length()) << (int)dist << " | ";
                }
                std::cerr << std::endl;
            }
        }

        m_arity_db.clear();
    }

    m_axioms.finalize();
    m_cdb_rhs.finalize();
    m_cdb_lhs.finalize();
    m_cdb_arg_set.finalize();
    m_cdb_arity_patterns.finalize();
    m_cdb_pattern_to_ids.finalize();
    m_rm.finalize();
    m_category_table.instance->finalize();
}


void knowledge_base_t::write_config() const
{
    std::string filename(m_filename + ".conf");
    std::ofstream fo(
        filename.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
    char version(NUM_OF_KB_VERSION_TYPES - 1); // LATEST VERSION
    char num_dp = m_distance_provider.key.length();
    char num_ct = m_category_table.key.length();

    if (not fo)
        throw phillip_exception_t(
        util::format("Cannot open KB-configuration file: \"%s\"", filename.c_str()));

    fo.write(&version, sizeof(char));
    fo.write((char*)&m_config_for_compile.max_distance, sizeof(float));

    fo.write(&num_dp, sizeof(char));
    fo.write(m_distance_provider.key.c_str(), m_distance_provider.key.length());
    m_distance_provider.instance->write(&fo);

    fo.write(&num_ct, sizeof(char));
    fo.write(m_category_table.key.c_str(), m_category_table.key.length());
    m_category_table.instance->write(&fo);

    fo.close();
}


void knowledge_base_t::read_config()
{
    std::string filename(m_filename + ".conf");
    std::ifstream fi(filename.c_str(), std::ios::in | std::ios::binary);
    char version, num;
    char key[256];

    if (not fi)
        throw phillip_exception_t(
        util::format("Cannot open KB-configuration file: \"%s\"", filename.c_str()));

    fi.read(&version, sizeof(char));

    if (version > 0 and version < NUM_OF_KB_VERSION_TYPES)
        m_version = static_cast<version_e>(version);
    else
    {
        m_version = KB_VERSION_UNDERSPECIFIED;
        throw phillip_exception_t(
            "This compiled knowledge base is invalid. Please re-compile it.");
    }

    if (not is_valid_version())
        throw phillip_exception_t(
        "This compiled knowledge base is too old. Please re-compile it.");

    fi.read((char*)&m_config_for_compile.max_distance, sizeof(float));

    fi.read(&num, sizeof(char));
    fi.read(key, num);
    key[num] = '\0';
    set_distance_provider(key);
    m_distance_provider.instance->read(&fi);

    fi.read(&num, sizeof(char));
    fi.read(key, num);
    key[num] = '\0';
    set_category_table(key);
    m_category_table.instance->read(&fi);

    fi.close();

}


axiom_id_t knowledge_base_t::insert_implication(
    const lf::logical_function_t &func, const std::string &name)
{
    if (m_state == STATE_COMPILE)
    {
        if (not func.is_valid_as_implication())
        {
            util::print_warning_fmt(
                "Axiom \"%s\" is invalid and skipped.", func.to_string().c_str());
            return INVALID_AXIOM_ID;
        }

        // ASSIGN ARITIES IN func TO ARITY-DATABASE.
        std::vector<const lf::logical_function_t*> branches;
        func.enumerate_literal_branches(&branches);
        for (auto br : branches)
            m_arity_db.add(br->literal().get_arity());

        // IF func IS CATEGORICAL KNOWLEDGE, IT IS INSERTED TO CATEGORY-TABLE.
        if (m_category_table.instance->insert(func))
            return INVALID_AXIOM_ID;

        axiom_id_t id = m_axioms.num_axioms();
        m_axioms.put(name, func);

        // REGISTER AXIOMS'S GROUPS
        auto spl = util::split(name, "#");
        if (spl.size() > 1)
        {
            for (int i = 0; i < spl.size() - 1; ++i)
                m_group_to_axioms[spl[i]].insert(id);
        }

        std::vector<const literal_t*> rhs(func.get_rhs());
        std::vector<const literal_t*> lhs(func.get_lhs());

        // ABDUCTION
        for (auto it = rhs.begin(); it != rhs.end(); ++it)
        if (not(*it)->is_equality())
        {
            arity_id_t arity_id = m_arity_db.add((*it)->get_arity());
            m_rhs_to_axioms[arity_id].insert(id);
        }

        // DEDUCTION
        if (m_config_for_compile.can_deduction)
        {
            for (auto it = lhs.begin(); it != lhs.end(); ++it)
            if (not(*it)->is_equality())
            {
                arity_id_t arity_id = m_arity_db.add((*it)->get_arity());
                m_lhs_to_axioms[arity_id].insert(id);
            }
        }

        return id;
    }
    else
        return INVALID_AXIOM_ID;
}


void knowledge_base_t::insert_inconsistency(const lf::logical_function_t &func)
{
    if (m_state == STATE_COMPILE)
    {
        if (not func.is_valid_as_inconsistency())
        {
            util::print_warning_fmt(
                "Inconsistency \"%s\" is invalid and skipped.",
                func.to_string().c_str());
            return;
        }
        else
        {
            std::vector<const lf::logical_function_t*> lits;
            func.enumerate_literal_branches(&lits);

            literal_t l1 = lits.at(0)->literal();
            literal_t l2 = lits.at(1)->literal();
            m_arity_db.add_mutual_exclusion(l1, l2);
        }
    }
}


void knowledge_base_t::insert_unification_postponement(const lf::logical_function_t &func)
{
    if (m_state == STATE_COMPILE)
    {
        if (not func.is_valid_as_unification_postponement())
        {
            util::print_warning_fmt(
                "Unification postponement \"%s\" is invalid and skipped.",
                func.to_string().c_str());
            return;
        }
        else
        {
            const term_t INDISPENSABLE("*"), PARTIAL("+"), DISPENSABLE(".");
            const literal_t &lit = func.branch(0).literal();

            arity_id_t arity = m_arity_db.add(lit.get_arity());
            std::vector<char> args(lit.terms.size(), 0);

            for (size_t i = 0; i < args.size(); ++i)
            {
                if (lit.terms.at(i) == INDISPENSABLE)
                    args[i] = UNI_PP_INDISPENSABLE;
                else if (lit.terms.at(i) == PARTIAL)
                    args[i] = UNI_PP_INDISPENSABLE_PARTIALLY;
                else if (lit.terms.at(i) == DISPENSABLE)
                    args[i] = UNI_PP_DISPENSABLE;
                else
                {
                    util::print_warning_fmt(
                        "The unification postponement for the arity \"%s\" is invalid.",
                        lit.get_arity().c_str());
                    return;
                }

            }

            int num(1);
            func.param2int(&num);

            m_arity_db.add_unification_postponement(unification_postponement_t(arity, args, num));
        }
    }
}


void knowledge_base_t::insert_argument_set(const lf::logical_function_t &f)
{
    if (m_state != STATE_COMPILE) return;

    if (not f.is_valid_as_argument_set())
    {
        util::print_warning_fmt(
            "Argument set \"%s\" is invalid and skipped.",
            f.to_string().c_str());
    }
    else
    {
        hash_set<std::string> *pivot = NULL;
        const std::vector<term_t> &terms(f.literal().terms);
        hash_set<std::string> args(terms.begin(), terms.end());

        for (auto it_set = m_argument_sets.begin(); it_set != m_argument_sets.end();)
        {
            bool do_match(false);

            for (auto a = args.begin(); a != args.end() and not do_match; ++a)
            if (it_set->count(*a))
                do_match = true;

            if (do_match)
            {
                if (pivot == NULL)
                {
                    pivot = &(*it_set);
                    pivot->insert(args.begin(), args.end());
                    ++it_set;
                }
                else
                {
                    pivot->insert(it_set->begin(), it_set->end());
                    it_set = m_argument_sets.erase(it_set);
                }
            }
            else ++it_set;
        }

        if (pivot == NULL)
            m_argument_sets.push_back(args);
    }
}


void knowledge_base_t::assert_stop_word(const arity_t &arity)
{
    m_asserted_stop_words.insert(arity);
}


hash_set<axiom_id_t> knowledge_base_t::search_axiom_group(axiom_id_t id) const
{
    hash_set<axiom_id_t> out{ id };
    auto found = m_axiom_group.axiom_to_groups.find(id);

    if (found != m_axiom_group.axiom_to_groups.end())
    for (auto grp : found->second)
        out.insert(grp->begin(), grp->end());

    return out;
}

argument_set_id_t knowledge_base_t::
search_argument_set_id(const std::string &arity, int term_idx) const
{
    if (not m_cdb_arg_set.is_readable())
    {
        util::print_warning("kb-search: Kb-state is invalid.");
        return 0;
    }

    std::string key = util::format("%s/%d", arity.c_str(), term_idx);
    size_t value_size;
    const argument_set_id_t *value = (const argument_set_id_t*)
        m_cdb_arg_set.get(key.c_str(), key.length(), &value_size);

    return (value == NULL) ? 0 : (*value);
}


void knowledge_base_t::search_arity_patterns(arity_id_t arity, std::list<arity_pattern_t> *out) const
{
    if (not m_cdb_arity_patterns.is_readable())
    {
        util::print_warning("kb-search: Kb-state is invalid.");
        return;
    }

    size_t value_size;
    const char *value = (const char*)
        m_cdb_arity_patterns.get(&arity, sizeof(arity_id_t), &value_size);

    if (value != NULL)
    {
        size_t num_query, read_size(0);
        read_size += util::binary_to<size_t>(value, &num_query);
        out->assign(num_query, arity_pattern_t());

        for (auto it = out->begin(); it != out->end(); ++it)
            read_size += binary_to_query(value + read_size, &(*it));
    }
}


void knowledge_base_t::search_axioms_with_arity_pattern(
    const arity_pattern_t &query,
    std::list<std::pair<axiom_id_t, bool> > *out) const
{
    if (not m_cdb_pattern_to_ids.is_readable())
    {
        util::print_warning("kb-search: Kb-state is invalid.");
        return;
    }

    std::vector<char> key;
    query_to_binary(query, &key);

    size_t value_size;
    const char *value = (const char*)
        m_cdb_pattern_to_ids.get(&key[0], key.size(), &value_size);

    if (value != NULL)
    {
        size_t size(0), num_id(0);
        size += util::binary_to<size_t>(value + size, &num_id);
        out->assign(num_id, std::pair<axiom_id_t, bool>());

        for (auto it = out->begin(); it != out->end(); ++it)
        {
            char flag;
            size += util::binary_to<axiom_id_t>(value + size, &(it->first));
            size += util::binary_to<char>(value + size, &flag);
            it->second = (flag != 0x00);
        }
    }
}


void knowledge_base_t::set_distance_provider(
    const std::string &key, const phillip_main_t *ph)
{
    if (m_distance_provider.instance != NULL)
        delete m_distance_provider.instance;

    m_distance_provider = {
        bin::distance_provider_library_t::instance()->generate(key, ph),
        key };

    if (m_distance_provider.instance == NULL)
        throw phillip_exception_t(
        util::format("The key of distance-provider is invalid: \"%s\"", key.c_str()));
}


void knowledge_base_t::set_category_table(
    const std::string &key, const phillip_main_t *ph)
{
    if (m_category_table.instance != NULL)
        delete m_category_table.instance;

    m_category_table = {
        bin::category_table_library_t::instance()->generate(key, ph),
        key };

    if (m_category_table.instance == NULL)
        throw phillip_exception_t(
        util::format("The key of category table is invalid: \"%s\"", key.c_str()));
}


float knowledge_base_t::get_distance(
    const std::string &arity1, const std::string &arity2 ) const
{
    arity_id_t get1 = search_arity_id(arity1);
    arity_id_t get2 = search_arity_id(arity2);
    if (get1 == INVALID_ARITY_ID or get2 == INVALID_ARITY_ID) return -1.0f;

    std::lock_guard<std::mutex> lock(ms_mutex_for_cache);
    auto found1 = m_cache_distance.find(get1);
    if (found1 != m_cache_distance.end())
    {
        auto found2 = found1->second.find(get2);
        if (found2 != found1->second.end())
            return found2->second;
    }

    float dist(m_rm.get(get1, get2));
    m_cache_distance[get1][get2] = dist;
    return dist;
}


void knowledge_base_t::write_axiom_group()
{
    std::ofstream fo(
        m_filename + ".group.dat",
        std::ios::binary | std::ios::trunc | std::ios::out);

    IF_VERBOSE_1("starts writing " + filename() + ".group.dat...");

    size_t size = m_group_to_axioms.size();
    fo.write((char*)&size, sizeof(size_t));

    for (auto p : m_group_to_axioms)
    {
        size_t n = p.second.size();
        fo.write((char*)&n, sizeof(size_t));

        for (auto a : p.second)
            fo.write((char*)&a, sizeof(axiom_id_t));
    }

    IF_VERBOSE_1("completed writing " + filename() + ".group.dat.");
}


void knowledge_base_t::read_axiom_group()
{
    std::ifstream fi(m_filename + ".group.dat", std::ios::binary | std::ios::in);

    size_t size;
    fi.read((char*)&size, sizeof(size_t));

    for (size_t i = 0; i < size; ++i)
    {
        m_axiom_group.groups.push_back(hash_set<axiom_id_t>());
        hash_set<axiom_id_t> &grp = m_axiom_group.groups.back();

        size_t n;
        fi.read((char*)&n, sizeof(size_t));

        for (size_t j = 0; j < n; ++j)
        {
            axiom_id_t id;
            fi.read((char*)&id, sizeof(axiom_id_t));
            grp.insert(id);
            m_axiom_group.axiom_to_groups[id].push_back(&grp);
        }
    }
}


void knowledge_base_t::insert_argument_set_to_cdb()
{
    IF_VERBOSE_1("starts writing " + m_cdb_arg_set.filename() + "...");
    IF_VERBOSE_4(util::format("  # of arg-sets = %d", m_argument_sets.size()));

    unsigned processed(0);
    for (auto args = m_argument_sets.begin(); args != m_argument_sets.end(); ++args)
    {
        argument_set_id_t id = (++processed);
        for (auto arg = args->begin(); arg != args->end(); ++arg)
            m_cdb_arg_set.put(arg->c_str(), arg->length(), &id, sizeof(argument_set_id_t));
    }

    IF_VERBOSE_1("completed writing " + m_cdb_arg_set.filename() + ".");
}


void knowledge_base_t::set_stop_words()
{
    bool can_use_lpsolve(false), can_use_gurobi(false);
#ifdef USE_LP_SOLVE
    can_use_lpsolve = true;
#endif
#ifdef USE_GUROBI
    can_use_gurobi = true;
#endif

    if (not can_use_gurobi and not can_use_lpsolve) return;
    if (m_config_for_compile.do_disable_stop_word) return;

    IF_VERBOSE_1("Setting stop-words...");
    m_axioms.prepare_query();

    typedef std::pair <std::string, char> term_pos_t;
    hash_map<arity_t, hash_set<term_idx_t> > candidates;
    std::set<term_pos_t> excluded;
    hash_map<std::string, size_t> counts; // ARITY FREQUENCY IN EVIDENCE
    std::set<std::list<std::string> > arities_set; // ARITY SET IN EVIDENCE
    hash_map<arity_t, hash_map<term_idx_t, std::list<std::string> > >
        names_of_axiom_excludes_expected_stop_word;

    auto proc = [&](const lf::axiom_t &ax, bool is_backward)
    {
        auto evd = is_backward ? ax.func.get_rhs() : ax.func.get_lhs();
        auto hyp = is_backward ? ax.func.get_lhs() : ax.func.get_rhs();
        hash_set<term_t> terms_evd;
        hash_map<term_t, std::list<term_pos_t> > hard_terms;
        std::set<std::string> arities;

        auto add_excluded = [&](const arity_t &a, term_idx_t i)
        {
            if (m_asserted_stop_words.count(a) > 0)
                names_of_axiom_excludes_expected_stop_word[a][i].push_back(ax.name);
            excluded.insert(std::make_pair(a, i));
        };
        
        for (auto l : evd)
        if (not l->is_equality())
        {
            std::string arity(l->get_arity());

            arities.insert(arity);
            (counts.count(arity) > 0) ? ++counts[arity] : (counts[arity] = 1);

            for (char i = 0; i < l->terms.size(); ++i)
            {
                term_t t(l->terms.at(i));
                terms_evd.insert(t);
                if (t.is_hard_term())
                    hard_terms[t].push_back(std::make_pair(arity, i));
                else
                    add_excluded(arity, i);
            }
        }

        std::list<std::string> arity_list(arities.begin(), arities.end());
        arity_list.sort();
        arities_set.insert(arity_list);

        for (auto e : hard_terms)
        if (e.second.size() > 1)
        {
            for (auto p : e.second)
                candidates[p.first].insert(p.second);
        }

        for (auto l : hyp)
        if (not l->is_equality())
        {
            for (char i = 0; i < l->terms.size(); ++i)
            if (terms_evd.count(l->terms.at(i)) > 0)
                add_excluded(l->get_arity(), i);
        }
    };

    for (axiom_id_t id = 0; id < m_axioms.num_axioms(); ++id)
    {
        lf::axiom_t ax = m_axioms.get(id);

        if (ax.func.is_operator(lf::OPR_IMPLICATION))
        {
            if (m_config_for_compile.can_deduction)
                proc(ax, false); // DEDUCTION
            proc(ax, true);  // ABDUCTION
        }


        if (id % 10 == 0 and phillip_main_t::verbose() >= VERBOSE_1)
        {
            float progress = (float)(id)* 100.0f / (float)m_axioms.num_axioms();
            std::cerr << util::format("processed %d axioms [%.4f%%]\r", id, progress);
        }
    }

    // CHECK WHETHER THERE ARE ASSERTED STOP-WORDS IN candidates
    {
        for (auto e : m_asserted_stop_words)
        {
            if (candidates.count(e) == 0)
                throw phillip_exception_t(util::format(
                "Stop-word assertion failed: "
                "\"%s\" is not a candidate of stop-word.", e.c_str()));
        }
    }

    // EXCLUDED ELEMENTS IN excluded FROM candidates
    for (auto it1 = candidates.begin(); it1 != candidates.end();)
    {
        for (auto it2 = it1->second.begin(); it2 != it1->second.end();)
        {
            term_pos_t p(it1->first, *it2);
        
            if (excluded.count(p) > 0)
            {
                if (m_asserted_stop_words.count(it1->first) > 0 and
                    it1->second.size() == 1)
                {
                    std::string ax_name =
                        names_of_axiom_excludes_expected_stop_word
                        .at(it1->first).at(*it2).front();
                    std::string disp(util::format(
                        "Stop-word assertion failed: "
                        "\"%s\" cannot be a stop-word because of \"%s\".",
                        it1->first.c_str(), ax_name.c_str()));
                
                    throw phillip_exception_t(disp);
                }
                it2 = it1->second.erase(it2);
            }
            else ++it2;
        }

        if (it1->second.empty())
            it1 = candidates.erase(it1);
        else ++it1;
    }

    if (candidates.empty()) return;

    hash_map<arity_t, ilp::variable_idx_t> a2v;
    for (auto c : candidates)
        a2v[c.first] = -1;

    ilp::ilp_problem_t prob(NULL, new ilp::basic_solution_interpreter_t(), true);

    for (auto it = a2v.begin(); it != a2v.end(); ++it)
    {
        double coef =
            (m_asserted_stop_words.count(it->first) > 0) ? 100.0 :
            100.0 * ((double)counts.at(it->first) - 0.9) / m_axioms.num_axioms();
        ilp::variable_t var(it->first, coef);
        it->second = prob.add_variable(var);
    }

    for (auto arities : arities_set)
    {
        bool do_add_constraint(true);

        for (auto a : arities)
        if (a2v.count(a) == 0)
            do_add_constraint = false;

        if (do_add_constraint)
        {
            ilp::constraint_t con("", ilp::OPR_LESS_EQ, 1.0 * (arities.size() - 1));
            for (auto a : arities)
                con.add_term(a2v.at(a), 1.0);
            prob.add_constraint(con);
        }
    }

    ilp_solver_t *solver = NULL;
    if (can_use_gurobi) solver = new sol::gurobi_t(NULL, m_config_for_compile.thread_num, false);
    else solver = new sol::lp_solve_t(NULL);

    std::vector<ilp::ilp_solution_t> solutions;
    solver->solve(&prob, &solutions);

    if (not solutions.empty())
    if (solutions.front().type() == ilp::SOLUTION_OPTIMAL)
    {
        for (auto it : a2v)
        {
            if (solutions.front().variable_is_active(it.second))
                m_stop_words.insert(it.first);
        }
    }

    delete solver;

    if (phillip_main_t::verbose() >= VERBOSE_3)
    {
        std::list<std::string> stop_words(m_stop_words.begin(), m_stop_words.end());
        stop_words.sort();
        
        util::print_console(
            "stop-words = {" +
            util::join(stop_words.begin(), stop_words.end(), ", ") + "}");
    }

    for (auto a : m_asserted_stop_words)
    {
        if (m_stop_words.count(a) == 0)
            throw phillip_exception_t(util::format(
            "Stop-word assertion failed: "
            "\"%s\" is not a stop-word.", a.c_str()));
    }
}


void knowledge_base_t::create_query_map()
{
    IF_VERBOSE_1("Creating the arity patterns...");

    m_axioms.prepare_query();
    m_cdb_rhs.prepare_query();
    m_cdb_lhs.prepare_query();

    std::map<arity_id_t, std::set<arity_pattern_t> > arity_to_queries;
    std::map<arity_pattern_t, std::set< std::pair<axiom_id_t, bool> > > pattern_to_ids;

    auto proc = [this, &pattern_to_ids, &arity_to_queries](
        const lf::axiom_t &ax, bool is_backward)
    {
        std::vector<const lf::logical_function_t*> branches;
        hash_map<string_hash_t, std::set<std::pair<arity_id_t, char> > > term2arity;
        arity_pattern_t query;

        ax.func.branch(is_backward ? 1 : 0).enumerate_literal_branches(&branches);

        for (index_t i = 0; i < branches.size(); ++i)
        {
            const literal_t &lit = branches[i]->literal();
            std::string arity = lit.get_arity();
            arity_id_t idx = search_arity_id(arity);

            assert(idx != INVALID_ARITY_ID);
            std::get<0>(query).push_back(idx);

            for (int i_t = 0; i_t < lit.terms.size(); ++i_t)
            if (lit.terms.at(i_t).is_hard_term())
                term2arity[lit.terms.at(i_t)].insert(std::make_pair(i, (char)i_t));
        }

        for (auto e : term2arity)
        {
            for (auto it1 = e.second.begin(); it1 != e.second.end(); ++it1)
            for (auto it2 = e.second.begin(); it2 != it1; ++it2)
                std::get<1>(query).push_back(util::make_sorted_pair(*it1, *it2));
        }
        std::get<1>(query).sort();

        for (char i = 0; i < branches.size(); ++i)
        if (category_table()->do_target(branches[i]->literal().get_arity()))
            std::get<2>(query).push_back(i);

        pattern_to_ids[query].insert(std::make_pair(ax.id, is_backward));

        for (auto idx : std::get<0>(query))
        if (m_stop_words.count(search_arity(idx)) == 0)
            arity_to_queries[idx].insert(query);
    };

    for (axiom_id_t i = 0; i < m_axioms.num_axioms(); ++i)
    {
        lf::axiom_t ax = get_axiom(i);

        if (ax.func.is_operator(lf::OPR_IMPLICATION))
        {
            if (m_config_for_compile.can_deduction)
                proc(ax, false); // DEDUCTION
            proc(ax, true);  // ABDUCTION
        }

        if (i % 10 == 0 and phillip_main_t::verbose() >= VERBOSE_1)
        {
            float progress = (float)(i)* 100.0f / (float)m_axioms.num_axioms();
            std::cerr << util::format("processed %d axioms [%.4f%%]\r", i, progress);
        }
    }

    m_cdb_arity_patterns.prepare_compile();
    IF_VERBOSE_2("  Writing " + m_cdb_arity_patterns.filename() + "...");

    for (auto p : arity_to_queries)
    {
        std::list< std::vector<char> > queries;
        size_t size_value(0);

        size_value += sizeof(size_t);
        for (auto q : p.second)
        {
            std::vector<char> bin;
            query_to_binary(q, &bin);
            queries.push_back(bin);
            size_value += bin.size();
        }

        char *value = new char[size_value];
        size_t size(0);

        size += util::to_binary<size_t>(p.second.size(), value);
        for (auto q : queries)
        {
            std::memcpy(value + size, &q[0], q.size());
            size += q.size();
        }

        assert(size == size_value);
        m_cdb_arity_patterns.put(
            (char*)(&p.first), sizeof(arity_id_t), value, size_value);

        delete[] value;
    }

    IF_VERBOSE_2("  Completed writing " + m_cdb_arity_patterns.filename() + ".");
    m_cdb_pattern_to_ids.prepare_compile();
    IF_VERBOSE_2("  Writing " + m_cdb_pattern_to_ids.filename() + "...");

    for (auto p : pattern_to_ids)
    {
        std::vector<char> key, val;
        query_to_binary(p.first, &key);

        size_t size_val = sizeof(size_t) + (sizeof(axiom_id_t) + sizeof(char)) * p.second.size();
        val.assign(size_val, '\0');

        size_t size = util::to_binary<size_t>(p.second.size(), &val[0]);
        for (auto p2 : p.second)
        {
            size += util::to_binary<axiom_id_t>(p2.first, &val[0] + size);
            size += util::to_binary<char>((p2.second ? 0xff : 0x00), &val[0] + size);
        }
        assert(size == size_val);

        m_cdb_pattern_to_ids.put(&key[0], key.size(), &val[0], val.size());
    }

    IF_VERBOSE_3(util::format("    # of patterns = %d", pattern_to_ids.size()));
    IF_VERBOSE_2("  Completed writing " + m_cdb_pattern_to_ids.filename() + ".");
    IF_VERBOSE_1("Completed the arity patterns creation.");
}


void knowledge_base_t::create_reachable_matrix()
{
    IF_VERBOSE_1("starts to create reachable matrix...");

    size_t processed(0), num_inserted(0);
    clock_t clock_past = clock_t();
    time_t time_start, time_end;
    time(&time_start);

    m_axioms.prepare_query();
    m_cdb_rhs.prepare_query();
    m_cdb_lhs.prepare_query();
    m_category_table.instance->prepare_query(this);

    m_rm.prepare_compile();

    IF_VERBOSE_3(util::format("  num of axioms = %d", m_axioms.num_axioms()));
    IF_VERBOSE_3(util::format("  num of arities = %d", m_arity_db.arities().size()));
    IF_VERBOSE_3(util::format("  max distance = %.2f", get_max_distance()));
    IF_VERBOSE_3(util::format("  num of parallel threads = %d", m_config_for_compile.thread_num));
    IF_VERBOSE_2("  computing distance of direct edges...");

    const std::vector<arity_t> &arities = m_arity_db.arities();
    hash_map<arity_id_t, hash_map<arity_id_t, float> > base_lhs, base_rhs;
    hash_set<arity_id_t> ignored;
    std::set<std::pair<arity_id_t, arity_id_t> > base_para;
    
    for (auto it = m_stop_words.begin(); it != m_stop_words.end(); ++it)
    {
        const arity_id_t idx = search_arity_id(*it);
        if (idx != INVALID_ARITY_ID) ignored.insert(idx);
    }
    ignored.insert(INVALID_ARITY_ID);
    
    _create_reachable_matrix_direct(ignored, &base_lhs, &base_rhs, &base_para);

    IF_VERBOSE_2("  writing reachable matrix...");
    std::vector<std::thread> worker;
    int num_thread =
        std::min<int>(arities.size(),
        std::min<int>(m_config_for_compile.thread_num,
        std::thread::hardware_concurrency()));
    
    for (int th_id = 0; th_id < num_thread; ++th_id)
    {
        worker.emplace_back(
            [&](int th_id)
            {
                for(arity_id_t idx = th_id; idx < arities.size(); idx += num_thread)
                {
                    if (ignored.count(idx) != 0) continue;
                    
                    hash_map<arity_id_t, float> dist;
                    _create_reachable_matrix_indirect(
                        idx, base_lhs, base_rhs, base_para, &dist);
                    m_rm.put(idx, dist);

                    ms_mutex_for_rm.lock();
                    
                    num_inserted += dist.size();
                    ++processed;

                    clock_t c = clock();
                    if (c - clock_past > CLOCKS_PER_SEC)
                    {
                        float progress = (float)(processed)* 100.0f / (float)arities.size();
                        std::cerr << util::format(
                            "processed %d tokens [%.4f%%]\r", processed, progress);
                        std::cerr.flush();
                        clock_past = c;
                    }
                    
                    ms_mutex_for_rm.unlock();
                }
            }, th_id);
    }
    for (auto &t : worker) t.join();
    
    time(&time_end);
    int proc_time(time_end - time_start); 
    double coverage(
        num_inserted * 100.0 /
        (double)(arities.size() * arities.size()));
    
    IF_VERBOSE_1("completed computation.");
    IF_VERBOSE_3(util::format("  process-time = %d", proc_time));
    IF_VERBOSE_3(util::format("  coverage = %.6lf%%", coverage));
}


void knowledge_base_t::_create_reachable_matrix_direct(
    const hash_set<arity_id_t> &ignored,
    hash_map<arity_id_t, hash_map<arity_id_t, float> > *out_lhs,
    hash_map<arity_id_t, hash_map<arity_id_t, float> > *out_rhs,
    std::set<std::pair<arity_id_t, arity_id_t> > *out_para)
{
    size_t num_processed(0);
    const std::vector<arity_t> &arities = m_arity_db.arities();

    // SET VALUES IN CATEGORY-TABLE TO REACHABLE-MATRIX
    for (arity_id_t i = 1; i < arities.size(); ++i)
    {
        hash_map<arity_id_t, float> buf;
        m_category_table.instance->gets(i, &buf);
        
        for (auto p : buf)
        if (p.second >= 0.0f)
        {
            (*out_lhs)[i][p.first] = p.second;
            (*out_rhs)[i][p.first] = p.second;
        }
    }

    for (arity_id_t i = 1; i < arities.size(); ++i)
    {
        if (ignored.count(i) == 0)
        {
            (*out_lhs)[i][i] = 0.0f;
            (*out_rhs)[i][i] = 0.0f;
        }
    }

    for (axiom_id_t id = 0; id < m_axioms.num_axioms(); ++id)
    {
        if (++num_processed % 10 == 0 and phillip_main_t::verbose() >= VERBOSE_1)
        {
            float progress = (float)(num_processed)* 100.0f / (float)m_axioms.num_axioms();
            std::cerr << util::format("processed %d axioms [%.4f%%]\r", num_processed, progress);
        }

        lf::axiom_t axiom = get_axiom(id);
        if (not axiom.func.is_operator(lf::OPR_IMPLICATION)) continue;

        float dist = (*m_distance_provider.instance)(axiom);
        if (dist >= 0.0f)
        {
            hash_set<arity_id_t> lhs_ids, rhs_ids;

            {
                std::vector<const literal_t*> lhs = axiom.func.get_lhs();
                std::vector<const literal_t*> rhs = axiom.func.get_rhs();

                for (auto it_l = lhs.begin(); it_l != lhs.end(); ++it_l)
                {
                    std::string arity = (*it_l)->get_arity();
                    arity_id_t idx = search_arity_id(arity);

                    if (idx != INVALID_ARITY_ID)
                    if (ignored.count(idx) == 0)
                        lhs_ids.insert(idx);
                }

                for (auto it_r = rhs.begin(); it_r != rhs.end(); ++it_r)
                {
                    std::string arity = (*it_r)->get_arity();
                    arity_id_t idx = search_arity_id(arity);

                    if (idx != INVALID_ARITY_ID)
                    if (ignored.count(idx) == 0)
                        rhs_ids.insert(idx);
                }
            }

            for (auto it_l = lhs_ids.begin(); it_l != lhs_ids.end(); ++it_l)
            {
                hash_map<arity_id_t, float> &target = (*out_lhs)[*it_l];
                for (auto it_r = rhs_ids.begin(); it_r != rhs_ids.end(); ++it_r)
                {
                    auto found = target.find(*it_r);
                    if (found == target.end())
                        target[*it_r] = dist;
                    else if (dist < found->second)
                        target[*it_r] = dist;
                }
            }

            for (auto it_r = rhs_ids.begin(); it_r != rhs_ids.end(); ++it_r)
            {
                hash_map<arity_id_t, float> &target = (*out_rhs)[*it_r];
                for (auto it_l = lhs_ids.begin(); it_l != lhs_ids.end(); ++it_l)
                {
                    auto found = target.find(*it_l);
                    if (found == target.end())
                        target[*it_l] = dist;
                    else if (dist < found->second)
                        target[*it_l] = dist;
                }
            }

            if (m_config_for_compile.can_deduction)
            {
                for (auto it_l = lhs_ids.begin(); it_l != lhs_ids.end(); ++it_l)
                for (auto it_r = rhs_ids.begin(); it_r != rhs_ids.end(); ++it_r)
                    out_para->insert(util::make_sorted_pair(*it_l, *it_r));
            }
        }
    }    
}


void knowledge_base_t::_create_reachable_matrix_indirect(
    arity_id_t target,
    const hash_map<arity_id_t, hash_map<arity_id_t, float> > &base_lhs,
    const hash_map<arity_id_t, hash_map<arity_id_t, float> > &base_rhs,
    const std::set<std::pair<arity_id_t, arity_id_t> > &base_para,
    hash_map<arity_id_t, float> *out) const
{
    if (base_lhs.count(target) == 0 or base_rhs.count(target) == 0) return;

    std::map<std::tuple<arity_id_t, bool, bool>, float> processed;

    std::function<void(arity_id_t, bool, bool, float, bool)> process = [&](
        arity_id_t idx1, bool can_abduction, bool can_deduction, float dist, bool is_forward)
    {
        const hash_map<arity_id_t, hash_map<arity_id_t, float> >
            &base = (is_forward ? base_lhs : base_rhs);
        auto found = base.find(idx1);

        if (found != base.end())
        for (auto it2 = found->second.begin(); it2 != found->second.end(); ++it2)
        {
            if (idx1 == it2->first) continue;
            if (it2->second < 0.0f) continue;

            arity_id_t idx2(it2->first);
            bool is_paraphrasal =
                (base_para.count(util::make_sorted_pair(idx1, idx2)) > 0);
            if (not is_paraphrasal and
                ((is_forward and not can_deduction) or
                (not is_forward and not can_abduction)))
                continue;

            float dist_new(dist + it2->second); // DISTANCE idx1 ~ idx2
            if (get_max_distance() >= 0.0f and dist_new > get_max_distance())
                continue;

            std::tuple<arity_id_t, bool, bool> key =
                std::make_tuple(idx2, can_abduction, can_deduction);

            // ONCE DONE DEDUCTION, THEN IT CANNOT DO ABDUCTION ANY MORE.
            if (not m_config_for_compile.can_deduction and is_forward)
                std::get<1>(key) = false;

            if (not util::find_then(processed, key, dist_new, std::less_equal<float>()))
            {
                processed[key] = dist_new;

                if (not util::find_then(*out, idx2, dist_new, std::less_equal<float>()))
                    (*out)[idx2] = dist_new;

                process(std::get<0>(key), std::get<1>(key), std::get<2>(key), dist_new, true);
                process(std::get<0>(key), std::get<1>(key), std::get<2>(key), dist_new, false);
            }
        }
    };

    processed[std::make_tuple(target, true, true)] = 0.0f;
    (*out)[target] = 0.0f;

    process(target, true, true, 0.0f, true);
    process(target, true, true, 0.0f, false);
}

// #define _DEV

void knowledge_base_t::extend_inconsistency()
{
#ifdef _DEV
    m_cdb_id.prepare_query();
    std::ofstream fo(
        (m_cdb_inc_pred.filename() + ".temporary").c_str(),
        std::ios::out | std::ios::trunc | std::ios::binary);

    hash_set<axiom_id_t> considered;

    for (auto it = m_inc_to_axioms.begin(); it != m_inc_to_axioms.end(); ++it)
    {
        for (auto ax = it->second.begin(); ax != it->second.end(); ++ax)
        {
            lf::axiom_t axiom = get_axiom(*ax);
            auto literals = axiom.func.get_all_literals();
            if (literals.size() != 2) continue;
        }
    }
#endif
}


void knowledge_base_t::_enumerate_deducible_literals(
    const literal_t &target, hash_set<literal_t> *out) const
{
}


std::list<axiom_id_t> knowledge_base_t::search_id_list(
    const std::string &query, const util::cdb_data_t *dat) const
{
    std::list<axiom_id_t> out;
    
    if (dat != NULL)
    {
        if (not dat->is_readable())
            util::print_warning("kb-search: Kb-state is invalid.");
        else
        {
            size_t value_size;
            const char *value = (const char*)
                dat->get(query.c_str(), query.length(), &value_size);

            if (value != NULL)
            {
                size_t size(0), num_id(0);
                size += util::binary_to<size_t>(value + size, &num_id);

                for (int j = 0; j<num_id; ++j)
                {
                    axiom_id_t id;
                    size += util::binary_to<axiom_id_t>(value + size, &id);
                    out.push_back(id);
                }
            }
        }
    }
    
    return out;
}


std::list<axiom_id_t> knowledge_base_t::search_id_list(
    arity_id_t arity_id, const util::cdb_data_t *dat) const
{
    std::list<axiom_id_t> out;

    if (arity_id != INVALID_ARITY_ID and dat != NULL)
    {
        if (not dat->is_readable())
            util::print_warning("kb-search: Kb-state is invalid.");
        else
        {
            size_t value_size;
            const char *value = (const char*)
                dat->get((char*)&arity_id, sizeof(arity_id_t), &value_size);

            if (value != NULL)
            {
                size_t size(0), num_id(0);
                size += util::binary_to<size_t>(value + size, &num_id);

                for (int j = 0; j<num_id; ++j)
                {
                    axiom_id_t id;
                    size += util::binary_to<axiom_id_t>(value + size, &id);
                    out.push_back(id);
                }
            }
        }
    }

    return out;
}



std::mutex knowledge_base_t::axioms_database_t::ms_mutex;

knowledge_base_t::axioms_database_t::axioms_database_t(const std::string &filename)
: m_filename(filename),
m_fo_idx(NULL), m_fo_dat(NULL), m_fi_idx(NULL), m_fi_dat(NULL),
m_num_compiled_axioms(0), m_num_unnamed_axioms(0)
{}


knowledge_base_t::axioms_database_t::~axioms_database_t()
{
    finalize();
}


void knowledge_base_t::axioms_database_t::prepare_compile()
{
    if (is_readable())
        finalize();

    if (not is_writable())
    {
        std::lock_guard<std::mutex> lock(ms_mutex);

        m_fo_idx = new std::ofstream(
            (m_filename + ".index.dat").c_str(), std::ios::binary | std::ios::out);
        m_fo_dat = new std::ofstream(
            (m_filename + ".axioms.dat").c_str(), std::ios::binary | std::ios::out);
        m_num_compiled_axioms = 0;
        m_num_unnamed_axioms = 0;
        m_writing_pos = 0;
    }
}


void knowledge_base_t::axioms_database_t::prepare_query()
{
    if (is_writable())
        finalize();

    if (not is_readable())
    {
        std::lock_guard<std::mutex> lock(ms_mutex);

        m_fi_idx = new std::ifstream(
            (m_filename + ".index.dat").c_str(), std::ios::binary | std::ios::in);
        m_fi_dat = new std::ifstream(
            (m_filename + ".axioms.dat").c_str(), std::ios::binary | std::ios::in);

        m_fi_idx->seekg(-static_cast<int>(sizeof(int)), std::ios_base::end);
        m_fi_idx->read((char*)&m_num_compiled_axioms, sizeof(int));
    }
}


void knowledge_base_t::axioms_database_t::finalize()
{
    if (is_writable())
        m_fo_idx->write((char*)&m_num_compiled_axioms, sizeof(int));

    if (m_fo_idx != NULL)
    {
        delete m_fo_idx;
        m_fo_idx = NULL;
    }

    if (m_fo_dat != NULL)
    {
        delete m_fo_dat;
        m_fo_dat = NULL;
    }

    if (m_fi_idx != NULL)
    {
        delete m_fi_idx;
        m_fi_idx = NULL;
    }

    if (m_fi_dat != NULL)
    {
        delete m_fi_dat;
        m_fi_dat = NULL;
    }
}


void knowledge_base_t::axioms_database_t::put(
    const std::string &name, const lf::logical_function_t &func)
{
    const int SIZE(512 * 512);
    char buffer[SIZE];

    /* AXIOM => BINARY-DATA */
    size_t size = func.write_binary(buffer);
    size += util::string_to_binary(
        (name.empty() ? get_name_of_unnamed_axiom() : name),
        buffer + size);
    assert(size < BUFFER_SIZE and size < ULONG_MAX);

    /* INSERT AXIOM TO CDB.ID */
    axiom_size_t _size(static_cast<axiom_size_t>(size));
    m_fo_idx->write((char*)(&m_writing_pos), sizeof(axiom_pos_t));
    m_fo_idx->write((char*)(&_size), sizeof(axiom_size_t));

    m_fo_dat->write(buffer, size);

    ++m_num_compiled_axioms;
    m_writing_pos += size;
}


lf::axiom_t knowledge_base_t::axioms_database_t::get(axiom_id_t id) const
{
    std::lock_guard<std::mutex> lock(ms_mutex);
    lf::axiom_t out;

    if (not is_readable())
    {
        util::print_warning("kb-search: KB is currently not readable.");
        return out;
    }

    axiom_pos_t pos;
    axiom_size_t size;
    const int SIZE(512 * 512);
    char buffer[SIZE];

    m_fi_idx->seekg(id * (sizeof(axiom_pos_t)+sizeof(axiom_size_t)));
    m_fi_idx->read((char*)&pos, sizeof(axiom_pos_t));
    m_fi_idx->read((char*)&size, sizeof(axiom_size_t));

    m_fi_dat->seekg(pos);
    m_fi_dat->read(buffer, size);

    out.id = id;
    size_t _size = out.func.read_binary(buffer);
    _size += util::binary_to_string(buffer + _size, &out.name);

    return out;
}



knowledge_base_t::arity_database_t::arity_database_t(const std::string &filename)
: m_filename(filename)
{
    m_arities.push_back("");
    m_arity2id[""] = INVALID_ARITY_ID;
}


void knowledge_base_t::arity_database_t::clear()
{
    m_arities.assign(1, "");

    m_arity2id.clear();
    m_arity2id[""] = INVALID_ARITY_ID;

    m_unification_postponements.clear();
    m_mutual_exclusions.clear();
}


void knowledge_base_t::arity_database_t::read()
{
    std::ifstream fi(m_filename.c_str(), std::ios::in | std::ios::binary);
    char line[256];

    clear();

    if (fi.bad())
        throw phillip_exception_t("Failed to open " + m_filename);

    size_t arity_num;
    fi.read((char*)&arity_num, sizeof(size_t));

    for (size_t i = 0; i < arity_num; ++i)
    {
        unsigned char num_char;
        fi.read((char*)&num_char, sizeof(unsigned char));
        fi.read(line, sizeof(char) * num_char);
        line[num_char] = '\0';

        add(arity_t(line));
    }

    size_t unipp_num;
    fi.read((char*)&unipp_num, sizeof(size_t));

    for (size_t i = 0; i < unipp_num; ++i)
        add_unification_postponement(unification_postponement_t(&fi));

    size_t muex_num_1;
    fi.read((char*)&muex_num_1, sizeof(size_t));

    for (size_t i = 0; i < muex_num_1; ++i)
    {
        size_t muex_num_2;
        arity_id_t id1;
        fi.read((char*)&id1, sizeof(arity_id_t));
        fi.read((char*)&muex_num_2, sizeof(size_t));

        for (size_t j = 0; j < muex_num_2; ++j)
        {
            arity_id_t id2;
            small_size_t pairs_num;
            std::list<std::pair<term_idx_t, term_idx_t> > pairs;

            fi.read((char*)&id2, sizeof(arity_id_t));
            fi.read((char*)&pairs_num, sizeof(small_size_t));

            for (small_size_t k = 0; k < pairs_num; ++k)
            {
                term_idx_t terms[2];
                fi.read((char*)terms, sizeof(term_idx_t) * 2);

                pairs.push_back(std::make_pair(terms[0], terms[1]));
            }

            m_mutual_exclusions[id1][id2] = pairs;
        }
    }
}


void knowledge_base_t::arity_database_t::write() const
{
    std::ofstream fo(m_filename.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
    char line[256];

    if (fo.bad())
        throw phillip_exception_t("Failed to open " + m_filename);

    size_t arity_num = m_arities.size();
    fo.write((char*)&arity_num, sizeof(size_t));

    for (auto arity : m_arities)
    {
        small_size_t num_char = arity.length();
        fo.write((char*)&num_char, sizeof(small_size_t));
        fo.write(arity.c_str(), sizeof(char)* num_char);
    }

    size_t unipp_num = m_unification_postponements.size();
    fo.write((char*)&unipp_num, sizeof(size_t));

    for (auto p : m_unification_postponements)
        p.second.write(&fo);

    size_t muex_num_1 = m_mutual_exclusions.size();
    fo.write((char*)&muex_num_1, sizeof(size_t));

    for (auto p1 : m_mutual_exclusions)
    {
        size_t muex_num_2 = p1.second.size();
        fo.write((char*)&p1.first, sizeof(arity_id_t));
        fo.write((char*)&muex_num_2, sizeof(size_t));

        for (auto p2 : p1.second)
        {
            small_size_t pairs_num = p2.second.size();
            fo.write((char*)&p2.first, sizeof(arity_id_t));
            fo.write((char*)&pairs_num, sizeof(small_size_t));

            for (auto p3 : p2.second)
            {
                term_idx_t terms[2] = { p3.first, p3.second };
                fo.write((char*)terms, sizeof(term_idx_t)* 2);
            }
        }
    }
}


void knowledge_base_t::arity_database_t::add_mutual_exclusion(const literal_t &l1, const literal_t &l2)
{
    std::list< std::pair<term_idx_t, term_idx_t> > pairs;

    for (term_idx_t t1 = 0; t1 < l1.terms.size(); ++t1)
    for (term_idx_t t2 = 0; t2 < l2.terms.size(); ++t2)
    {
        if (l1.terms.at(t1) == l2.terms.at(t2))
            pairs.push_back(std::make_pair(t1, t2));
    }

    if (not pairs.empty())
    {
        arity_id_t a1 = add(l1.get_arity());
        arity_id_t a2 = add(l2.get_arity());
        if (a1 > a2) std::swap(a1, a2);

        m_mutual_exclusions[a1][a2] = pairs;
    }
}



std::mutex knowledge_base_t::reachable_matrix_t::ms_mutex;


knowledge_base_t::reachable_matrix_t::reachable_matrix_t(const std::string &filename)
    : m_filename(filename), m_fout(NULL), m_fin(NULL)
{}


knowledge_base_t::reachable_matrix_t::~reachable_matrix_t()
{
    finalize();
}


void knowledge_base_t::reachable_matrix_t::prepare_compile()
{
    if (is_readable())
        finalize();

    if (not is_writable())
    {
        std::lock_guard<std::mutex> lock(ms_mutex);
        pos_t pos;
        
        m_fout = new std::ofstream(
            m_filename.c_str(), std::ios::binary | std::ios::out);
        m_fout->write((const char*)&pos, sizeof(pos_t));
    }
}


void knowledge_base_t::reachable_matrix_t::prepare_query()
{
    if (is_writable())
        finalize();

    if (not is_readable())
    {
        std::lock_guard<std::mutex> lock(ms_mutex);
        pos_t pos;
        size_t num, idx;

        m_fin = new std::ifstream(
            m_filename.c_str(), std::ios::binary | std::ios::in);

        m_fin->read((char*)&pos, sizeof(pos_t));
        m_fin->seekg(pos, std::ios::beg);

        m_fin->read((char*)&num, sizeof(size_t));
        for (size_t i = 0; i < num; ++i)
        {
            m_fin->read((char*)&idx, sizeof(idx));
            m_fin->read((char*)&pos, sizeof(pos_t));
            m_map_idx_to_pos[idx] = pos;
        }
    }
}


void knowledge_base_t::reachable_matrix_t::finalize()
{
    if (m_fout != NULL)
    {
        std::lock_guard<std::mutex> lock(ms_mutex);
        pos_t pos = m_fout->tellp();
        size_t num = m_map_idx_to_pos.size();

        m_fout->write((const char*)&num, sizeof(size_t));

        for (auto it = m_map_idx_to_pos.begin(); it != m_map_idx_to_pos.end(); ++it)
        {
            m_fout->write((const char*)&it->first, sizeof(size_t));
            m_fout->write((const char*)&it->second, sizeof(pos_t));
        }

        m_fout->seekp(0, std::ios::beg);
        m_fout->write((const char*)&pos, sizeof(pos_t));

        delete m_fout;
        m_fout = NULL;
    }

    if (m_fin != NULL)
    {
        delete m_fin;
        m_fin = NULL;
    }

    m_map_idx_to_pos.clear();
}


void knowledge_base_t::reachable_matrix_t::
put(size_t idx1, const hash_map<size_t, float> &dist)
{
    std::lock_guard<std::mutex> lock(ms_mutex);
    size_t num(0);
    m_map_idx_to_pos[idx1] = m_fout->tellp();

    for (auto it = dist.begin(); it != dist.end(); ++it)
        if (idx1 <= it->first)
            ++num;

    m_fout->write((const char*)&num, sizeof(size_t));
    for (auto it = dist.begin(); it != dist.end(); ++it)
    {
        if (idx1 <= it->first)
        {
            m_fout->write((const char*)&it->first, sizeof(size_t));
            m_fout->write((const char*)&it->second, sizeof(float));
        }
    }
}


float knowledge_base_t::reachable_matrix_t::get(size_t idx1, size_t idx2) const
{
    if (idx1 > idx2) std::swap(idx1, idx2);

    std::lock_guard<std::mutex> lock(ms_mutex);
    size_t num, idx;
    float dist;
    auto find = m_map_idx_to_pos.find(idx1);

    if (find == m_map_idx_to_pos.end()) return -1.0f;

    m_fin->seekg(find->second, std::ios::beg);
    m_fin->read((char*)&num, sizeof(size_t));

    for (size_t i = 0; i < num; ++i)
    {
        m_fin->read((char*)&idx, sizeof(size_t));
        m_fin->read((char*)&dist, sizeof(float));
        if (idx == idx2) return dist;
    }

    return -1.0f;
}


hash_set<float> knowledge_base_t::reachable_matrix_t::get(size_t idx) const
{
    std::lock_guard<std::mutex> lock(ms_mutex);
    size_t num;
    float dist;
    hash_set<float> out;
    auto find = m_map_idx_to_pos.find(idx);

    if (find == m_map_idx_to_pos.end()) return out;

    m_fin->seekg(find->second, std::ios::beg);
    m_fin->read((char*)&num, sizeof(size_t));

    for (size_t i = 0; i < num; ++i)
    {
        m_fin->read((char*)&idx, sizeof(size_t));
        m_fin->read((char*)&dist, sizeof(float));
        out.insert(dist);
    }

    return out;
}


namespace dist
{

float basic_distance_provider_t::operator()(const lf::axiom_t &ax) const
{
    float out;
    return ax.func.scan_parameter("d%f", &out) ? out : 1.0f;
}


float cost_based_distance_provider_t::operator()(const lf::axiom_t &ax) const
{
    float out(-1.0f);
    return ax.func.scan_parameter("%f", &out) ? out : 1.0f;
}

}


namespace ct
{


bool null_category_table_t::insert(const lf::logical_function_t&)
{
    return false;
}


float null_category_table_t::get(const arity_t &a1, const arity_t &a2) const
{
    return (a1 == a2) ? 0.0 : -1.0;
}


float null_category_table_t::get(arity_id_t a1, arity_id_t a2) const
{
    assert(a1 != INVALID_ARITY_ID and a2 != INVALID_ARITY_ID);
    return (a1 == a2) ? 0.0 : -1.0;
}


bool null_category_table_t::do_target(const arity_t&) const
{
    return false;
}


category_table_t* basic_category_table_t::
generator_t::operator() (const phillip_main_t *ph) const
{
    // NOTE: Currently ph is always NULL.

    if (ph)
        return new basic_category_table_t(
            ph->param_int("ct_max_depth", -1),
            ph->param_float("ct_distance_scale", 1.0f));
    else
        return new basic_category_table_t(2, 1.0);
}


basic_category_table_t::basic_category_table_t(int max_depth, float dist_scale)
    : m_max_depth(max_depth), m_distance_scale(dist_scale)
{
    assert(m_distance_scale > 0.0f);
}


void basic_category_table_t::read(std::ifstream *fi)
{
    fi->read((char*)&m_distance_scale, sizeof(float));
}


void basic_category_table_t::write(std::ofstream *fo) const
{
    fo->write((char*)&m_distance_scale, sizeof(float));
}


void basic_category_table_t::prepare_compile(const knowledge_base_t *base)
{
    if (m_state != STATE_NULL)
        finalize();

    m_prefix = base->filename();
    m_state = STATE_COMPILE;
}


bool basic_category_table_t::insert(const lf::logical_function_t &ax)
{
    assert(m_state == STATE_COMPILE);

    if (do_insert(ax))
    {
        std::vector<const lf::logical_function_t*> lhs, rhs;
        ax.branch(0).enumerate_literal_branches(&lhs);
        ax.branch(1).enumerate_literal_branches(&rhs);

        arity_id_t a1 = kb()->search_arity_id(lhs.front()->literal().get_arity());
        arity_id_t a2 = kb()->search_arity_id(rhs.front()->literal().get_arity());

        assert(a1 != INVALID_ARITY_ID and a2 != INVALID_ARITY_ID);
        m_table[a1][a2] = m_distance_scale; // DEDUCTION
        m_table[a2][a1] = m_distance_scale; // ABDUCTION

        return true;
    }
    else
        return false;
}


void basic_category_table_t::prepare_query(const kb::knowledge_base_t *base)
{
    if (m_state != STATE_NULL)
        finalize();

    m_prefix = base->filename();
    m_state = STATE_QUERY;

    read(filename());
}


float basic_category_table_t::get(const arity_t &a1, const arity_t &a2) const
{
    assert(m_state == STATE_QUERY);

    if (a1 == a2) return 0.0f;

    arity_id_t i = kb()->search_arity_id(a1);
    arity_id_t j = kb()->search_arity_id(a2);

    return (i != INVALID_ARITY_ID and j != INVALID_ARITY_ID) ? get(i, j) : -1.0f;
}


float basic_category_table_t::get(arity_id_t a1, arity_id_t a2) const
{
    assert(a1 != INVALID_ARITY_ID and a2 != INVALID_ARITY_ID);

    auto found1 = m_table.find(a1);
    if (found1 != m_table.end())
    {
        auto found2 = found1->second.find(a2);
        if (found2 != found1->second.end())
            return found2->second;
    }

    return -1.0f;
}


void basic_category_table_t::gets(
    const arity_id_t &a1, hash_map<arity_id_t, float> *out) const
{
    auto find = m_table.find(a1);
    if (find != m_table.end())
        out->insert(find->second.begin(), find->second.end());
}


bool basic_category_table_t::do_insert(
    const lf::logical_function_t &func) const
{
    if (func.is_valid_as_implication())
    {
        auto lhs = func.get_lhs();
        auto rhs = func.get_rhs();

        if (lhs.size() == 1 and rhs.size() == 1)
        {
            term_t t1 = lhs.front()->terms.front();
            term_t t2 = rhs.front()->terms.front();
            return t1 == t2;
        }
        else
            return false;
    }
    else
        return false;
}


bool basic_category_table_t::do_target(const arity_t &a) const
{
    int num;
    if (util::parse_arity(a, NULL, &num))
        return (num == 1);
    else
        return false;
}


void basic_category_table_t::finalize()
{
    if (m_state == STATE_COMPILE)
    {
        combinate();
        write(filename());
    }

    m_table.clear();
    m_state = STATE_NULL;
}


void basic_category_table_t::combinate()
{
    const float max_dist = kb()->get_max_distance();
    
    auto search = [this](arity_id_t a1, arity_id_t a2) -> float
    {
        auto found1 = m_table.find(a1);
        if (found1 != m_table.end())
        {
            auto found2 = found1->second.find(a2);
            if (found2 != found1->second.end())
                return found2->second;
        }
        return -1.0f;
    };
    
    std::function<void(arity_id_t, arity_id_t, float, int)> walk =
        [&, this](arity_id_t a1, arity_id_t a2, float dist, int depth)
    {
        if (m_max_depth >= 0 and depth >= m_max_depth)
            return;
        
        auto it = m_table.find(a2);
        if (it == m_table.end()) return;

        for (auto p : it->second)
        {
            if (p.first != a1)
            {
                float d_new = dist + p.second;
                float d_old = search(a1, p.first);

                if (d_old < 0.0 or d_new < d_old)
                if (max_dist < 0.0 or d_new < max_dist)
                {
                    m_table[a1][p.first] = d_new;
                    walk(a1, p.first, d_new, depth + 1);
                }
            }
        }
    };

    IF_VERBOSE_1("Constructing category-table...");
    IF_VERBOSE_3(util::format("    max-distance = %.2f", max_dist));
    IF_VERBOSE_3(util::format("    max-depth = %d", m_max_depth));
    IF_VERBOSE_3(util::format("    distance-scale = %.2f", m_distance_scale));

    int n(0);
    for (auto p1 : m_table)
    {
        for (auto p2 : p1.second)
            walk(p1.first, p2.first, p2.second, 1);

        float rate = 100.0f * (++n) / m_table.size();
        if (n % 10 == 0 and phillip_main_t::verbose() >= VERBOSE_1)
            std::cerr << "Processed " << n << " elements [" << rate << "%]\r";
    }
    
    IF_VERBOSE_1("Completed category-table construction.");
}


void basic_category_table_t::write(const std::string &filename) const
{
    std::ofstream fout(
        filename, std::ios::out | std::ios::trunc | std::ios::binary);

    if (fout.bad())
    {
        util::print_error_fmt("Cannot open %s.", filename.c_str());
        return;
    }

    size_t num1 = m_table.size();
    fout.write((char*)&num1, sizeof(size_t));
    
    IF_VERBOSE_4("Writing basic-category-table.");

    size_t num(0);
    for (auto p1 : m_table)
    {
        size_t num2 = p1.second.size();
        fout.write((char*)&p1.first, sizeof(arity_id_t));
        fout.write((char*)&num2, sizeof(size_t));

        for (auto p2 : p1.second)
        {
            fout.write((char*)&p2.first, sizeof(arity_id_t));
            fout.write((char*)&p2.second, sizeof(float));
            ++num;
        }
    }
    
    IF_VERBOSE_4(util::format("    # of entities = %d", num));
}


void basic_category_table_t::read(const std::string &filename)
{
    std::ifstream fin(filename, std::ios::in | std::ios::binary);
    size_t num1, num2, num(0);
    arity_id_t id1, id2;
    m_table.clear();

    if (fin.bad())
    {
        util::print_error_fmt("Cannot open %s.", filename.c_str());
        return;
    }

    fin.read((char*)&num1, sizeof(size_t));

    IF_VERBOSE_4("Reading basic-category-table.");

    for (int i = 0; i < num1; ++i)
    {
        fin.read((char*)&id1, sizeof(arity_id_t));
        fin.read((char*)&num2, sizeof(size_t));

        for (int j = 0; j < num2; ++j)
        {
            float d;
            fin.read((char*)&id2, sizeof(arity_id_t));
            fin.read((char*)&d, sizeof(float));
            m_table[id1][id2] = d;
            ++num;
        }
    }

    IF_VERBOSE_4(util::format("    # of entities = %d", num));
}


}


void query_to_binary(const arity_pattern_t &q, std::vector<char> *bin)
{
    size_t size_expected =
        sizeof(unsigned char) * 3 +
        sizeof(arity_id_t) * std::get<0>(q).size() +
        (sizeof(arity_id_t) + sizeof(char)) * 2 * std::get<1>(q).size() +
        sizeof(char) * std::get<2>(q).size();
    size_t size = 0;
    bin->assign(size_expected, '\0');

    size += util::num_to_binary(std::get<0>(q).size(), &(*bin)[0]);
    for (auto id : std::get<0>(q))
        size += util::to_binary<arity_id_t>(id, &(*bin)[0] + size);

    size += util::num_to_binary(std::get<1>(q).size(), &(*bin)[0] + size);
    for (auto ht : std::get<1>(q))
    {
        size += util::to_binary<arity_id_t>(ht.first.first, &(*bin)[0] + size);
        size += util::to_binary<char>(ht.first.second, &(*bin)[0] + size);
        size += util::to_binary<arity_id_t>(ht.second.first, &(*bin)[0] + size);
        size += util::to_binary<char>(ht.second.second, &(*bin)[0] + size);
    }

    size += util::num_to_binary(std::get<2>(q).size(), &(*bin)[0] + size);
    for (auto i : std::get<2>(q))
        (*bin)[size++] = i;

    assert(size == size_expected);
};


size_t binary_to_query(const char *bin, arity_pattern_t *out)
{
    size_t size(0);
    int num_arity, num_hardterm, num_option;

    std::get<0>(*out).clear();
    std::get<1>(*out).clear();
    std::get<2>(*out).clear();

    size += util::binary_to_num(bin + size, &num_arity);
    for (int i = 0; i < num_arity; ++i)
    {
        arity_id_t id;
        size += util::binary_to<arity_id_t>(bin + size, &id);
        std::get<0>(*out).push_back(id);
    }

    size += util::binary_to_num(bin + size, &num_hardterm);
    for (int i = 0; i < num_hardterm; ++i)
    {
        arity_id_t id1, id2;
        char idx1, idx2;
        size += util::binary_to<arity_id_t>(bin + size, &id1);
        size += util::binary_to<char>(bin + size, &idx1);
        size += util::binary_to<arity_id_t>(bin + size, &id2);
        size += util::binary_to<char>(bin + size, &idx2);
        std::get<1>(*out).push_back(std::make_pair(
            std::make_pair(id1, idx1), std::make_pair(id2, idx2)));
    }

    size += util::binary_to_num(bin + size, &num_option);
    for (int i = 0; i < num_option; ++i)
        std::get<2>(*out).push_back(bin[size++]);

    return size;
}


} // end kb

} // end phil

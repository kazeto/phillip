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
std::unique_ptr<knowledge_base_t, deleter_t<knowledge_base_t> > knowledge_base_t::ms_instance;
std::string knowledge_base_t::ms_filename = "kb";
float knowledge_base_t::ms_max_distance = -1.0f;
int knowledge_base_t::ms_thread_num_for_rm = 1;
bool knowledge_base_t::ms_do_disable_stop_word = false;
std::mutex knowledge_base_t::ms_mutex_for_cache;
std::mutex knowledge_base_t::ms_mutex_for_rm;


knowledge_base_t* knowledge_base_t::instance()
{
    if (not ms_instance)
    {
        mkdir(get_directory_name(ms_filename));
        ms_instance.reset(new knowledge_base_t(ms_filename));
    }
    return ms_instance.get();
}


void knowledge_base_t::setup(
    std::string filename, float max_distance,
    int thread_num_for_rm, bool do_disable_stop_word)
{
    if (not ms_instance)
    {
        ms_filename = filename;
        ms_max_distance = max_distance;
        ms_thread_num_for_rm = thread_num_for_rm;
        ms_do_disable_stop_word = do_disable_stop_word;

        if (ms_thread_num_for_rm < 0) ms_thread_num_for_rm = 1;
    }
    else
        print_warning("Failed to setup. The instance of KB has been created.");
}


knowledge_base_t::knowledge_base_t(const std::string &filename)
    : m_state(STATE_NULL),
      m_filename(filename), m_version(KB_VERSION_1), 
      m_cdb_rhs(filename + ".rhs.cdb"),
      m_cdb_lhs(filename + ".lhs.cdb"),
      m_cdb_axiom_group(filename + ".group.cdb"),
      m_cdb_arg_set(filename + ".args.cdb"),
      m_cdb_arity_to_queries(filename + ".pattern.cdb"),
      m_cdb_query_to_ids(filename + ".search.cdb"),
      m_axioms(filename),
      m_arity_db(filename + ".arity.dat"),
      m_rm(filename + ".rm.dat")
{
    m_distance_provider = { NULL, "" };
    m_category_table = { NULL, "" };
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
        m_cdb_axiom_group.prepare_compile();
        m_cdb_arg_set.prepare_compile();
        m_cdb_arity_to_queries.prepare_compile();
        m_cdb_query_to_ids.prepare_compile();
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
        m_arity_db.read();

        m_axioms.prepare_query();
        m_cdb_rhs.prepare_query();
        m_cdb_lhs.prepare_query();
        m_cdb_axiom_group.prepare_query();
        m_cdb_arg_set.prepare_query();
        m_cdb_arity_to_queries.prepare_query();
        m_cdb_query_to_ids.prepare_query();
        m_rm.prepare_query();
        m_category_table.instance->prepare_query(this);

        m_state = STATE_QUERY;
    }
}


void knowledge_base_t::finalize()
{
    if (m_state == STATE_NULL) return;

    if (m_state == STATE_COMPILE)
    {
        auto insert_cdb = [](
            const hash_map<arity_id_t, hash_set<axiom_id_t> > &ids,
            cdb_data_t *dat)
        {
            print_console("starts writing " + dat->filename() + "...");

            for (auto it = ids.begin(); it != ids.end(); ++it)
            {
                size_t read_size = sizeof(size_t)+sizeof(axiom_id_t)* it->second.size();
                char *buffer = new char[read_size];

                int size = to_binary<size_t>(it->second.size(), buffer);
                for (auto id = it->second.begin(); id != it->second.end(); ++id)
                    size += to_binary<axiom_id_t>(*id, buffer + size);

                assert(read_size == size);
                dat->put((char*)&it->first, sizeof(arity_id_t), buffer, size);
                delete[] buffer;
            }

            print_console("completed writing " + dat->filename() + ".");
        };

        extend_inconsistency();

        insert_cdb(m_rhs_to_axioms, &m_cdb_rhs);
        insert_cdb(m_lhs_to_axioms, &m_cdb_lhs);
        insert_axiom_group_to_cdb();
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
    m_cdb_axiom_group.finalize();
    m_cdb_arg_set.finalize();
    m_cdb_arity_to_queries.finalize();
    m_cdb_query_to_ids.finalize();
    m_rm.finalize();
    m_category_table.instance->finalize();

    m_state = STATE_NULL;
}


void knowledge_base_t::write_config() const
{
    std::string filename(m_filename + ".conf");
    std::ofstream fo(
        filename.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
    char version(NUM_OF_KB_VERSION_TYPES - 1); // LATEST VERSION
    char num = m_distance_provider.key.length();

    fo.write(&version, sizeof(char));
    fo.write((char*)&ms_max_distance, sizeof(float));
    fo.write(&num, sizeof(char));
    fo.write(m_distance_provider.key.c_str(), m_distance_provider.key.length());

    fo.close();
}


void knowledge_base_t::read_config()
{
    std::string filename(m_filename + ".conf");
    std::ifstream fi(filename.c_str(), std::ios::in | std::ios::binary);
    char version, num;
    char key[256];

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

    fi.read((char*)&ms_max_distance, sizeof(float));
    fi.read(&num, sizeof(char));
    fi.read(key, num);

    fi.close();

    key[num] = '\0';
    set_distance_provider(key);
}


axiom_id_t knowledge_base_t::insert_implication(
    const lf::logical_function_t &func, const std::string &name)
{
    if (m_state == STATE_COMPILE)
    {
        bool is_implication = func.is_valid_as_implication();
        bool is_paraphrase = func.is_valid_as_paraphrase();

        if (not is_implication and not is_paraphrase)
        {
            print_warning_fmt(
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
        auto spl = split(name, "#");
        if (spl.size() > 1)
        {
            for (int i = 0; i < spl.size() - 1; ++i)
                m_group_to_axioms[spl[i]].insert(id);
        }

        std::vector<const literal_t*> rhs(func.get_rhs());
        std::vector<const literal_t*> lhs(func.get_lhs());

        for (auto it = rhs.begin(); it != rhs.end(); ++it)
        if (not (*it)->is_equality())
        {
            arity_id_t arity_id = m_arity_db.add((*it)->get_arity());
            m_rhs_to_axioms[arity_id].insert(id);
        }

        for (auto it = lhs.begin(); it != lhs.end(); ++it)
        if (not(*it)->is_equality())
        {
            arity_id_t arity_id = m_arity_db.add((*it)->get_arity());
            if (is_paraphrase)
                m_lhs_to_axioms[arity_id].insert(id);
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
            print_warning_fmt(
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
            print_warning_fmt(
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
                    print_warning_fmt(
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
        print_warning_fmt(
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


hash_set<axiom_id_t> knowledge_base_t::search_axiom_group(axiom_id_t id) const
{
    std::string key = format("#%lu", id);
    hash_set<axiom_id_t> out;

    if (not m_cdb_axiom_group.is_readable())
    {
        print_warning("kb-search: Kb-state is invalid.");
        return out;
    }

    size_t value_size;
    const char *value = (const char*)
        m_cdb_axiom_group.get(key.c_str(), key.length(), &value_size);

    if (value == NULL) return out;

    size_t size(0), num_grp(0);
    size += binary_to<size_t>(value + size, &num_grp);

    for (int i = 0; i < num_grp; ++i)
    {
        std::string grp;
        size += binary_to_string(value + size, &grp);

        auto ids = search_id_list(grp, &m_cdb_axiom_group);
        out.insert(ids.begin(), ids.end());
    }

    return out;
}

argument_set_id_t knowledge_base_t::
search_argument_set_id(const std::string &arity, int term_idx) const
{
    if (not m_cdb_arg_set.is_readable())
    {
        print_warning("kb-search: Kb-state is invalid.");
        return 0;
    }

    std::string key = format("%s/%d", arity.c_str(), term_idx);
    size_t value_size;
    const argument_set_id_t *value = (const argument_set_id_t*)
        m_cdb_arg_set.get(key.c_str(), key.length(), &value_size);

    return (value == NULL) ? 0 : (*value);
}


void knowledge_base_t::search_queries(arity_id_t arity, std::list<search_query_t> *out) const
{
    if (not m_cdb_arity_to_queries.is_readable())
    {
        print_warning("kb-search: Kb-state is invalid.");
        return;
    }

    size_t value_size;
    const char *value = (const char*)
        m_cdb_arity_to_queries.get(&arity, sizeof(arity_id_t), &value_size);

    if (value != NULL)
    {
        size_t num_query, read_size(0);
        read_size += binary_to<size_t>(value, &num_query);
        out->assign(num_query, search_query_t());

        for (auto it = out->begin(); it != out->end(); ++it)
            read_size += binary_to_query(value + read_size, &(*it));
    }
}


void knowledge_base_t::search_axioms_with_query(
    const search_query_t &query,
    std::list<std::pair<axiom_id_t, bool> > *out) const
{
    if (not m_cdb_query_to_ids.is_readable())
    {
        print_warning("kb-search: Kb-state is invalid.");
        return;
    }

    std::vector<char> key;
    query_to_binary(query, &key);

    size_t value_size;
    const char *value = (const char*)
        m_cdb_query_to_ids.get(&key[0], key.size(), &value_size);

    if (value != NULL)
    {
        size_t size(0), num_id(0);
        size += binary_to<size_t>(value + size, &num_id);
        out->assign(num_id, std::pair<axiom_id_t, bool>());

        for (auto it = out->begin(); it != out->end(); ++it)
        {
            char flag;
            size += binary_to<axiom_id_t>(value + size, &(it->first));
            size += binary_to<char>(value + size, &flag);
            it->second = (flag != 0x00);
        }
    }
}


void knowledge_base_t::set_distance_provider(const std::string &key)
{
    if (m_distance_provider.instance != NULL)
        delete m_distance_provider.instance;

    m_distance_provider = {
        bin::distance_provider_library_t::instance()->generate(key, NULL),
        key };

    if (m_distance_provider.instance == NULL)
        print_error_fmt(
        "The key of distance-provider is invalid: \"%s\"", key.c_str());
}


void knowledge_base_t::set_category_table(const std::string &key)
{
    if (m_category_table.instance != NULL)
        delete m_category_table.instance;

    m_category_table = {
        bin::category_table_library_t::instance()->generate(key, NULL),
        key };

    if (m_category_table.instance == NULL)
        print_error_fmt(
        "The key of category table is invalid: \"%s\"", key.c_str());
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


void knowledge_base_t::insert_axiom_group_to_cdb()
{
    cdb_data_t &dat(m_cdb_axiom_group);
    const hash_map<std::string, hash_set<axiom_id_t> >& map(m_group_to_axioms);
    hash_map<axiom_id_t, hash_set<std::string> > axiom_to_group;

    print_console("starts writing " + dat.filename() + "...");

    for (auto it = map.begin(); it != map.end(); ++it)
    {
        size_t byte_size = sizeof(size_t)+sizeof(axiom_id_t)* it->second.size();
        char *buffer = new char[byte_size];

        int size = to_binary<size_t>(it->second.size(), buffer);
        for (auto id = it->second.begin(); id != it->second.end(); ++id)
        {
            size += to_binary<axiom_id_t>(*id, buffer + size);
            axiom_to_group[*id].insert(it->first);
        }

        assert(byte_size == size);
        
        dat.put(it->first.c_str(), it->first.length(), buffer, size);
        delete [] buffer;
    }

    const int SIZE(512 * 512);
    char buffer[SIZE];
    
    for (auto it = axiom_to_group.begin(); it != axiom_to_group.end(); ++it)
    {
        int size = to_binary<size_t>(it->second.size(), buffer);
        for (auto grp : it->second)
            size += string_to_binary(grp, buffer + size);

        assert(size < SIZE);

        std::string key = format("#%lu", it->first);
        dat.put(key.c_str(), key.length(), buffer, size);
    }

    print_console("completed writing " + dat.filename() + ".");
}


void knowledge_base_t::insert_argument_set_to_cdb()
{
    print_console("starts writing " + m_cdb_arg_set.filename() + "...");
    IF_VERBOSE_4(format("  # of arg-sets = %d", m_argument_sets.size()));

    unsigned processed(0);
    for (auto args = m_argument_sets.begin(); args != m_argument_sets.end(); ++args)
    {
        argument_set_id_t id = (++processed);
        for (auto arg = args->begin(); arg != args->end(); ++arg)
            m_cdb_arg_set.put(arg->c_str(), arg->length(), &id, sizeof(argument_set_id_t));
    }

    print_console("completed writing " + m_cdb_arg_set.filename() + ".");
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

    if (not can_use_lpsolve and not can_use_lpsolve) return;
    if (ms_do_disable_stop_word) return;

    print_console("Setting stop-words...");
    m_axioms.prepare_query();

    typedef std::pair <std::string, char> term_pos_t;
    std::set<term_pos_t> candidates, excluded;
    hash_map<std::string, size_t> counts; // ARITY FREQUENCY IN EVIDENCE
    std::set<std::list<std::string> > arities_set; // ARITY SET IN EVIDENCE

    auto proc = [&](const lf::axiom_t &ax, bool is_backward)
    {
        auto evd = is_backward ? ax.func.get_rhs() : ax.func.get_lhs();
        auto hyp = is_backward ? ax.func.get_lhs() : ax.func.get_rhs();
        hash_set<term_t> terms_evd;
        hash_map<term_t, std::list<term_pos_t> > hard_terms;
        std::set<std::string> arities;

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
                    excluded.insert(std::make_pair(arity, i));
            }
        }

        std::list<std::string> arity_list(arities.begin(), arities.end());
        arity_list.sort();
        arities_set.insert(arity_list);

        for (auto e : hard_terms)
        if (e.second.size() > 1)
            candidates.insert(e.second.begin(), e.second.end());

        for (auto l : hyp)
        if (not l->is_equality())
        {
            for (char i = 0; i < l->terms.size(); ++i)
            if (terms_evd.count(l->terms.at(i)) > 0)
                excluded.insert(std::make_pair(l->get_arity(), i));
        }
    };

    for (axiom_id_t id = 0; id < m_axioms.num_axioms(); ++id)
    {
        lf::axiom_t ax = m_axioms.get(id);

        if (ax.func.is_operator(lf::OPR_IMPLICATION))
            proc(ax, true);
        else if (ax.func.is_operator(lf::OPR_PARAPHRASE))
        {
            proc(ax, true);
            proc(ax, false);
        }

        if (id % 10 == 0)
        {
            float progress = (float)(id)* 100.0f / (float)m_axioms.num_axioms();
            std::cerr << format("processed %d axioms [%.4f%%]\r", id, progress);
        }
    }

    // EXCLUDED ELEMENTS IN excluded FROM candidate
    for (auto it = candidates.begin(); it != candidates.end();)
    {
        if (excluded.count(*it) > 0)
            it = candidates.erase(it);
        else ++it;
    }

    if (candidates.empty()) return;

    hash_map<std::string, ilp::variable_idx_t> a2v;
    for (auto c : candidates)
        a2v[c.first] = -1;

    ilp::ilp_problem_t prob(NULL, new ilp::basic_solution_interpreter_t(), true);

    for (auto it = a2v.begin(); it != a2v.end(); ++it)
    {
        double coef =
            100.0 * ((double)counts.at(it->first) - 0.9)
            / m_axioms.num_axioms();
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
    if (can_use_gurobi) solver = new sol::gurobi_t(NULL, ms_thread_num_for_rm, false);
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

    IF_VERBOSE_3(
        "stop-words = {" +
        join(m_stop_words.begin(), m_stop_words.end(), ", ") + "}");
}


void knowledge_base_t::create_query_map()
{
    print_console("Creating the query map...");

    m_axioms.prepare_query();
    m_cdb_rhs.prepare_query();
    m_cdb_lhs.prepare_query();

    std::map<arity_id_t, std::set<search_query_t> > arity_to_queries;
    std::map<search_query_t, std::set< std::pair<axiom_id_t, bool> > > query_to_ids;

    auto proc = [this, &query_to_ids, &arity_to_queries](
        const lf::axiom_t &ax, bool is_backward)
    {
        std::vector<const lf::logical_function_t*> branches;
        std::list<std::string> arities;
        hash_map<string_hash_t, std::set<std::pair<arity_id_t, char> > > term2arity;

        ax.func.branch(is_backward ? 1 : 0).enumerate_literal_branches(&branches);

        for (auto br : branches)
        {
            const literal_t &lit = br->literal();
            std::string arity = lit.get_arity();
            arity_id_t idx = search_arity_id(arity);
            assert(idx != INVALID_ARITY_ID);
            arities.push_back(arity);

            for (int i_t = 0; i_t < lit.terms.size(); ++i_t)
            if (lit.terms.at(i_t).is_hard_term())
                term2arity[lit.terms.at(i_t)].insert(std::make_pair(idx, (char)i_t));
        }

        search_query_t query;
        for (auto a : arities)
        {
            arity_id_t idx = search_arity_id(a);
            std::get<0>(query).push_back(idx);
        }
        std::get<0>(query).sort();

        for (auto e : term2arity)
        {
            for (auto it1 = e.second.begin(); it1 != e.second.end(); ++it1)
            for (auto it2 = e.second.begin(); it2 != it1; ++it2)
                std::get<1>(query).push_back(make_sorted_pair(*it1, *it2));
        }
        std::get<1>(query).sort();

        for (char i = 0; i < branches.size(); ++i)
        if (do_target_on_category_table(branches[i]->literal().get_arity()))
            std::get<2>(query).push_back(i);

        query_to_ids[query].insert(std::make_pair(ax.id, is_backward));

        for (auto a : arities)
        if (m_stop_words.count(a) == 0)
        {
            arity_id_t idx = search_arity_id(a);
            arity_to_queries[idx].insert(query);
        }
    };

    for (axiom_id_t i = 0; i < m_axioms.num_axioms(); ++i)
    {
        lf::axiom_t ax = get_axiom(i);

        if (ax.func.is_operator(lf::OPR_IMPLICATION))
            proc(ax, true);
        else if (ax.func.is_operator(lf::OPR_PARAPHRASE))
        {
            proc(ax, true);
            proc(ax, false);
        }

        if (i % 10 == 0)
        {
            float progress = (float)(i)* 100.0f / (float)m_axioms.num_axioms();
            std::cerr << format("processed %d axioms [%.4f%%]\r", i, progress);
        }
    }

    m_cdb_arity_to_queries.prepare_compile();
    print_console("  Writing " + m_cdb_arity_to_queries.filename() + "...");

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

        size += to_binary<size_t>(p.second.size(), value);
        for (auto q : queries)
        {
            std::memcpy(value + size, &q[0], q.size());
            size += q.size();
        }

        assert(size == size_value);
        m_cdb_arity_to_queries.put(
            (char*)(&p.first), sizeof(arity_id_t), value, size_value);

        delete[] value;
    }

    print_console("  Completed writing " + m_cdb_arity_to_queries.filename() + ".");
    m_cdb_query_to_ids.prepare_compile();
    print_console("  Writing " + m_cdb_query_to_ids.filename() + "...");

    for (auto p : query_to_ids)
    {
        std::vector<char> key, val;

        query_to_binary(p.first, &key);

        size_t size_val = sizeof(size_t) + (sizeof(axiom_id_t) + sizeof(char)) * p.second.size();
        val.assign(size_val, '\0');

        size_t size = to_binary<size_t>(p.second.size(), &val[0]);
        for (auto p2 : p.second)
        {
            size += to_binary<axiom_id_t>(p2.first, &val[0] + size);
            size += to_binary<char>((p2.second ? 0xff : 0x00), &val[0] + size);
        }
        assert(size == size_val);

        m_cdb_query_to_ids.put(&key[0], key.size(), &val[0], val.size());
    }

    print_console_fmt("    # of queries = %d", query_to_ids.size());
    print_console("  Completed writing " + m_cdb_query_to_ids.filename() + ".");
    print_console("Completed the query map creation.");
}


void knowledge_base_t::create_reachable_matrix()
{
    print_console("starts to create reachable matrix...");

    size_t processed(0), num_inserted(0);
    clock_t clock_past = clock_t();
    time_t time_start, time_end;
    time(&time_start);

    m_axioms.prepare_query();
    m_cdb_rhs.prepare_query();
    m_cdb_lhs.prepare_query();
    m_category_table.instance->prepare_query(this);

    m_rm.prepare_compile();

    print_console_fmt("  num of axioms = %d", m_axioms.num_axioms());
    print_console_fmt("  num of arities = %d", m_arity_db.arities().size());
    print_console_fmt("  max distance = %.2f", get_max_distance());
    print_console_fmt("  num of parallel threads = %d", ms_thread_num_for_rm);
    print_console("  computing distance of direct edges...");

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

    print_console("  writing reachable matrix...");
    std::vector<std::thread> worker;
    int num_thread =
        std::min<int>(arities.size(),
        std::min<int>(ms_thread_num_for_rm, std::thread::hardware_concurrency()));
    
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
                        std::cerr << format(
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
    
    print_console("completed computation.");
    print_console_fmt("  process-time = %d", proc_time);
    print_console_fmt("  coverage = %.6lf%%", coverage);
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
        const arity_t &ar1 = arities.at(i);
        for (arity_id_t j = 1; j < arities.size(); ++j)
        {
            const arity_t &ar2 = arities.at(j);
            float d = m_category_table.instance->get(ar1, ar2);
            if (d >= 0.0f)
            {
                (*out_lhs)[i][j] = d;
                (*out_rhs)[i][j] = d;
            }
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
        lf::axiom_t axiom = get_axiom(id);

        if (axiom.func.is_operator(lf::OPR_IMPLICATION) or
            axiom.func.is_operator(lf::OPR_PARAPHRASE))
        {
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

                if (axiom.func.is_operator(lf::OPR_PARAPHRASE))
                for (auto it_l = lhs_ids.begin(); it_l != lhs_ids.end(); ++it_l)
                for (auto it_r = rhs_ids.begin(); it_r != rhs_ids.end(); ++it_r)
                    out_para->insert(make_sorted_pair(*it_l, *it_r));
            }
        }

        if (++num_processed % 10 == 0)
        {
            float progress = (float)(num_processed)* 100.0f / (float)m_axioms.num_axioms();
            std::cerr << format("processed %d axioms [%.4f%%]\r", num_processed, progress);
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

    std::map<std::tuple<arity_id_t, bool, bool>, float> current;
    std::map<std::tuple<arity_id_t, bool, bool>, float> processed;

    current[std::make_tuple(target, true, true)] = 0.0f;
    processed[std::make_tuple(target, true, true)] = 0.0f;
    (*out)[target] = 0.0f;

    while (not current.empty())
    {
        std::map<std::tuple<arity_id_t, bool, bool>, float> next;
        auto _process = [&](
            arity_id_t idx1, bool can_abduction, bool can_deduction,
            float dist, bool is_forward)
        {
            const hash_map<arity_id_t, hash_map<arity_id_t, float> >
                &base = (is_forward ? base_lhs : base_rhs);
            auto found = base.find(idx1);

            if (found != base.end())
            for (auto it2 = found->second.begin(); it2 != found->second.end(); ++it2)
            {
                arity_id_t idx2(it2->first);
                if (idx1 == idx2) continue;

                bool is_paraphrasal =
                    (base_para.count(make_sorted_pair(idx1, idx2)) > 0);
                if (not is_paraphrasal and
                    ((is_forward and not can_deduction) or
                    (not is_forward and not can_abduction)))
                    continue;

                float dist_new(dist + it2->second); // DISTANCE idx1 ~ idx2
                if (get_max_distance() < 0.0f or dist_new <= get_max_distance())
                {
                    std::tuple<arity_id_t, bool, bool> key =
                        std::make_tuple(idx2, can_abduction, can_deduction);

                    // ONCE DONE DEDUCTION, YOU CANNOT DO ABDUCTION!
                    if (is_forward and not is_paraphrasal) std::get<1>(key) = false;

                    bool do_add(false);
                    auto found = processed.find(key);
                    if (found == processed.end())  do_add = true;
                    else if (dist_new < found->second) do_add = true;

                    if (do_add)
                    {
                        next[key] = dist_new;
                        processed[key] = dist_new;

                        auto found_out = out->find(idx2);
                        if (found_out == out->end())           (*out)[idx2] = dist_new;
                        else if (dist_new < found_out->second) (*out)[idx2] = dist_new;
                    }
                }
            }
        };

        for (auto it1 = current.begin(); it1 != current.end(); ++it1)
        {
            arity_id_t idx = std::get<0>(it1->first);
            bool can_abduction = std::get<1>(it1->first);
            bool can_deduction = std::get<2>(it1->first);

            _process(idx, can_abduction, can_deduction, it1->second, false);
            _process(idx, can_abduction, can_deduction, it1->second, true);
        }

        current = next;
    }
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
    const std::string &query, const cdb_data_t *dat) const
{
    std::list<axiom_id_t> out;
    
    if (dat != NULL)
    {
        if (not dat->is_readable())
            print_warning("kb-search: Kb-state is invalid.");
        else
        {
            size_t value_size;
            const char *value = (const char*)
                dat->get(query.c_str(), query.length(), &value_size);

            if (value != NULL)
            {
                size_t size(0), num_id(0);
                size += binary_to<size_t>(value + size, &num_id);

                for (int j = 0; j<num_id; ++j)
                {
                    axiom_id_t id;
                    size += binary_to<axiom_id_t>(value + size, &id);
                    out.push_back(id);
                }
            }
        }
    }
    
    return out;
}


std::list<axiom_id_t> knowledge_base_t::search_id_list(
    arity_id_t arity_id, const cdb_data_t *dat) const
{
    std::list<axiom_id_t> out;

    if (arity_id != INVALID_ARITY_ID and dat != NULL)
    {
        if (not dat->is_readable())
            print_warning("kb-search: Kb-state is invalid.");
        else
        {
            size_t value_size;
            const char *value = (const char*)
                dat->get((char*)&arity_id, sizeof(arity_id_t), &value_size);

            if (value != NULL)
            {
                size_t size(0), num_id(0);
                size += binary_to<size_t>(value + size, &num_id);

                for (int j = 0; j<num_id; ++j)
                {
                    axiom_id_t id;
                    size += binary_to<axiom_id_t>(value + size, &id);
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
    size += string_to_binary(
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
        print_warning("kb-search: KB is currently not readable.");
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
    _size += binary_to_string(buffer + _size, &out.name);

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
    auto splitted = split(ax.func.param(), ":");
    float dist;

    for (auto s : splitted)
    {
        if (_sscanf(s.c_str(), "d%f", &dist) > 0)
            return dist;
    }

    return 1.0f;
}


float cost_based_distance_provider_t::operator()(const lf::axiom_t &ax) const
{
    const std::string &param = ax.func.param();
    float out(-1.0f);
    _sscanf(param.substr(1).c_str(), "%f", &out);
    return out;
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


bool null_category_table_t::do_target(const arity_t&) const
{
    return false;
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

        m_table[a1][a2] = 1;
        m_table[a2][a1] = 1;

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

    auto found1 = m_table.find(i);
    if (found1 != m_table.end())
    {
        auto found2 = found1->second.find(j);
        if (found2 != found1->second.end())
            return found2->second;
    }

    return -1.0f;
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
    if (parse_arity(a, NULL, &num))
        return (num == 1);
    else
        return false;
}


void basic_category_table_t::finalize()
{
    if (m_state == STATE_COMPILE)
        write(filename());

    m_table.clear();
    m_state = STATE_NULL;
}


void basic_category_table_t::combinate()
{
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

    while (true)
    {
        for (auto p1 : m_table)
        for (auto p2 : p1.second)
        {
            for (auto q1 : m_table.at(p1.first))
            for (auto q2 : m_table.at(p2.first))
            {
                float d_old = search(p1.first, p2.first);
                float d_new = p2.second + q1.second + q2.second;

                if (d_old < 0.0f or(d_old >= 0.0f and d_new < d_old))
                {
                    // TODO
                }
            }
        }
    }
}


void basic_category_table_t::write(const std::string &filename) const
{
    std::ofstream fout(
        filename, std::ios::out | std::ios::trunc | std::ios::binary);

    if (fout.bad())
    {
        print_error_fmt("Cannot open %s.", filename.c_str());
        return;
    }

    size_t num1 = m_table.size();
    fout.write((char*)&num1, sizeof(size_t));

    for (auto p1 : m_table)
    {
        size_t num2 = p1.second.size();
        fout.write((char*)&p1.first, sizeof(arity_id_t));
        fout.write((char*)&num2, sizeof(size_t));

        for (auto p2 : p1.second)
        {
            fout.write((char*)&p2.first, sizeof(arity_id_t));
            fout.write((char*)&p2.second, sizeof(float));
        }
    }
}


void basic_category_table_t::read(const std::string &filename)
{
    std::ifstream fin(filename, std::ios::in | std::ios::binary);
    size_t num1, num2;
    arity_id_t id1, id2;
    m_table.clear();

    if (fin.bad())
    {
        print_error_fmt("Cannot open %s.", filename.c_str());
        return;
    }

    fin.read((char*)&num1, sizeof(size_t));

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
        }
    }
}


}


void query_to_binary(const search_query_t &q, std::vector<char> *bin)
{
    size_t size_expected =
        sizeof(unsigned char) * 3 +
        sizeof(arity_id_t) * std::get<0>(q).size() +
        (sizeof(arity_id_t) + sizeof(char)) * 2 * std::get<1>(q).size() +
        sizeof(char) * std::get<2>(q).size();
    size_t size = 0;
    bin->assign(size_expected, '\0');

    size += num_to_binary(std::get<0>(q).size(), &(*bin)[0]);
    for (auto id : std::get<0>(q))
        size += to_binary<arity_id_t>(id, &(*bin)[0] + size);

    size += num_to_binary(std::get<1>(q).size(), &(*bin)[0] + size);
    for (auto ht : std::get<1>(q))
    {
        size += to_binary<arity_id_t>(ht.first.first, &(*bin)[0] + size);
        size += to_binary<char>(ht.first.second, &(*bin)[0] + size);
        size += to_binary<arity_id_t>(ht.second.first, &(*bin)[0] + size);
        size += to_binary<char>(ht.second.second, &(*bin)[0] + size);
    }

    size += num_to_binary(std::get<2>(q).size(), &(*bin)[0] + size);
    for (auto i : std::get<2>(q))
        (*bin)[size++] = i;

    assert(size == size_expected);
};


size_t binary_to_query(const char *bin, search_query_t *out)
{
    size_t size(0);
    int num_arity, num_hardterm, num_option;

    std::get<0>(*out).clear();
    std::get<1>(*out).clear();
    std::get<2>(*out).clear();

    size += binary_to_num(bin + size, &num_arity);
    for (int i = 0; i < num_arity; ++i)
    {
        arity_id_t id;
        size += binary_to<arity_id_t>(bin + size, &id);
        std::get<0>(*out).push_back(id);
    }

    size += binary_to_num(bin + size, &num_hardterm);
    for (int i = 0; i < num_hardterm; ++i)
    {
        arity_id_t id1, id2;
        char idx1, idx2;
        size += binary_to<arity_id_t>(bin + size, &id1);
        size += binary_to<char>(bin + size, &idx1);
        size += binary_to<arity_id_t>(bin + size, &id2);
        size += binary_to<char>(bin + size, &idx2);
        std::get<1>(*out).push_back(std::make_pair(
            std::make_pair(id1, idx1), std::make_pair(id2, idx2)));
    }

    size += binary_to_num(bin + size, &num_option);
    for (int i = 0; i < num_option; ++i)
        std::get<2>(*out).push_back(bin[size++]);

    return size;
}


} // end kb

} // end phil

/* -*- coding: utf-8 -*- */

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


namespace phil
{

namespace kb
{


unification_postponement_t::unification_postponement_t(
    const std::string &arity, const std::vector<char> &args,
    int num_for_partial_indispensability)
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


bool unification_postponement_t::do_postpone(
    const pg::proof_graph_t *graph, index_t n1, index_t n2) const
{
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
}


const int BUFFER_SIZE = 512 * 512;
std::unique_ptr<knowledge_base_t, knowledge_base_t::deleter> knowledge_base_t::ms_instance;
std::string knowledge_base_t::ms_filename = "kb";
distance_provider_type_e knowledge_base_t::ms_distance_provider_type = DISTANCE_PROVIDER_BASIC;
float knowledge_base_t::ms_max_distance = -1.0f;
bool knowledge_base_t::ms_do_compute_distance_for_abduction = true;
bool knowledge_base_t::ms_do_compute_distance_for_deduction = true;
int knowledge_base_t::ms_thread_num_for_rm = 1;
std::mutex knowledge_base_t::ms_mutex_for_cache;
std::mutex knowledge_base_t::ms_mutex_for_rm;


knowledge_base_t* knowledge_base_t::instance()
{
    if (not ms_instance)
        ms_instance.reset(new knowledge_base_t(ms_filename, ms_distance_provider_type));
    return ms_instance.get();
}


void knowledge_base_t::setup(
    std::string filename, distance_provider_type_e dist_type,
    float max_distance, int thread_num_for_rm,
    bool do_compute_for_abduction, bool do_compute_for_deduction)
{
    if (not ms_instance)
    {
        ms_filename = filename;
        ms_distance_provider_type = dist_type;
        ms_max_distance = max_distance;
        ms_thread_num_for_rm = thread_num_for_rm;
        ms_do_compute_distance_for_abduction = do_compute_for_abduction;
        ms_do_compute_distance_for_deduction = do_compute_for_deduction;

        if (ms_thread_num_for_rm < 0) ms_thread_num_for_rm = 1;
    }
    else
        print_error("Failed to setup. The instance of KB has been created.");
}


knowledge_base_t::knowledge_base_t(
    const std::string &filename, distance_provider_type_e dist)
    : m_state(STATE_NULL),
      m_filename(filename), m_version(KB_VERSION_1), 
      m_cdb_name(filename +".name.cdb"),
      m_cdb_rhs(filename + ".rhs.cdb"),
      m_cdb_lhs(filename + ".lhs.cdb"),
      m_cdb_inc_pred(filename + ".inc.pred.cdb"),
      m_cdb_axiom_group(filename + ".group.cdb"),
      m_cdb_uni_pp(filename + ".unipp.cdb"),
      m_cdb_arg_set(filename + ".args.cdb"),
      m_cdb_rm_idx(filename + ".rm.cdb"),
      m_axioms(filename), m_rm(filename + ".rm.dat"),
      m_rm_dist(new basic_distance_provider_t())
{
    set_distance_provider(dist);
}


knowledge_base_t::~knowledge_base_t()
{
    finalize();
    delete m_rm_dist;
}


void knowledge_base_t::prepare_compile()
{
    if (m_state == STATE_QUERY)
        finalize();

    if (m_state == STATE_NULL)
    {
        m_axioms.prepare_compile();
        m_cdb_name.prepare_compile();
        m_cdb_rhs.prepare_compile();
        m_cdb_lhs.prepare_compile();
        m_cdb_inc_pred.prepare_compile();
        m_cdb_uni_pp.prepare_compile();
        m_cdb_axiom_group.prepare_compile();
        m_cdb_arg_set.prepare_compile();
        m_cdb_rm_idx.prepare_compile();

        m_state = STATE_COMPILE;
    }
}


void knowledge_base_t::prepare_query()
{
    if (m_state == STATE_COMPILE)
        finalize();

    if (m_state == STATE_NULL)
    {
        read_config((m_filename + ".conf").c_str());

        m_axioms.prepare_query();
        m_cdb_name.prepare_query();
        m_cdb_rhs.prepare_query();
        m_cdb_lhs.prepare_query();
        m_cdb_inc_pred.prepare_query();
        m_cdb_uni_pp.prepare_query();
        m_cdb_axiom_group.prepare_query();
        m_cdb_arg_set.prepare_query();
        m_cdb_rm_idx.prepare_query();
        m_rm.prepare_query();

        m_state = STATE_QUERY;
    }
}


void knowledge_base_t::finalize()
{
    if (m_state == STATE_NULL) return;

    if (m_state == STATE_COMPILE)
    {
        extend_inconsistency();

        _insert_cdb(m_name_to_axioms, &m_cdb_name);
        _insert_cdb(m_rhs_to_axioms, &m_cdb_rhs);
        _insert_cdb(m_lhs_to_axioms, &m_cdb_lhs);
        _insert_cdb(m_inc_to_axioms, &m_cdb_inc_pred);
        _insert_cdb(m_arity_to_postponement, &m_cdb_uni_pp);
        insert_axiom_group_to_cdb();
        insert_argument_set_to_cdb();

        m_name_to_axioms.clear();
        m_rhs_to_axioms.clear();
        m_lhs_to_axioms.clear();
        m_inc_to_axioms.clear();
        m_group_to_axioms.clear();
        m_arity_to_postponement.clear();
        m_argument_sets.clear();

        create_reachable_matrix();
        write_config((m_filename + ".conf").c_str());

        m_arity_set.clear();
    }

    m_axioms.finalize();
    m_cdb_name.finalize();
    m_cdb_rhs.finalize();
    m_cdb_lhs.finalize();
    m_cdb_inc_pred.finalize();
    m_cdb_uni_pp.finalize();
    m_cdb_axiom_group.finalize();
    m_cdb_arg_set.finalize();
    m_cdb_rm_idx.finalize();
    m_rm.finalize();

    m_state = STATE_NULL;
}


void knowledge_base_t::write_config(const char *filename) const
{
    std::ofstream fo(
        filename, std::ios::out | std::ios::trunc | std::ios::binary);
    char dist_type(m_rm_dist->type());
    char version(KB_VERSION_3);

    fo.write(&version, sizeof(char));
    fo.write((char*)&ms_max_distance, sizeof(float));
    fo.write(&dist_type, sizeof(char));

    char flag_abduction(ms_do_compute_distance_for_abduction ? 0xff : 0x00);
    char flag_deduction(ms_do_compute_distance_for_deduction ? 0xff : 0x00);
    fo.write(&flag_abduction, sizeof(char));
    fo.write(&flag_deduction, sizeof(char));

    fo.close();
}


void knowledge_base_t::read_config(const char *filename)
{
    std::ifstream fi(filename, std::ios::in | std::ios::binary);
    char dist_type, version, flag_abduction, flag_deduction;

    fi.read(&version, sizeof(char));

    if (version > 0 and version < NUM_OF_KB_VERSION_TYPES)
        m_version = static_cast<version_e>(version);
    else
    {
        m_version = KB_VERSION_UNDERSPECIFIED;
        print_error(
            "This compiled knowledge base is invalid. Please re-compile it.");
        return;
    }

    if (m_version != KB_VERSION_3)
    {
        print_error(
            "This compiled knowledge base is too old. Please re-compile it.");
        return;
    }

    fi.read((char*)&ms_max_distance, sizeof(float));
    fi.read(&dist_type, sizeof(char));
    fi.read(&flag_abduction, sizeof(char));
    fi.read(&flag_deduction, sizeof(char));
    fi.close();

    ms_do_compute_distance_for_abduction = (flag_abduction != 0x00);
    ms_do_compute_distance_for_deduction = (flag_deduction != 0x00);
    ms_distance_provider_type = static_cast<distance_provider_type_e>(dist_type);
    set_distance_provider(ms_distance_provider_type);
}


void knowledge_base_t::insert_implication(
    const lf::logical_function_t &lf, const std::string &name)
{
    if (m_state == STATE_COMPILE)
    {
        if (not lf.is_valid_as_implication())
        {
            print_warning_fmt(
                "Implication \"%s\" is invalid and skipped.",
                lf.to_string().c_str());
            return;
        }

        axiom_id_t id = m_axioms.num_axioms();
        m_axioms.put(name, lf);
        m_name_to_axioms[name].insert(id);

        // REGISTER AXIOMS'S GROUPS
        auto spl = split(name, "#");
        if (spl.size() > 1)
        {
            for (int i = 0; i < spl.size() - 1; ++i)
                m_group_to_axioms[spl[i]].insert(id);
        }

        std::vector<const literal_t*> rhs(lf.get_rhs()), lhs(lf.get_lhs());

        for (auto it = rhs.begin(); it != rhs.end(); ++it)
        {
            if (not (*it)->is_equality())
            {
                std::string arity((*it)->get_predicate_arity());
                m_rhs_to_axioms[arity].insert(id);
                insert_arity(arity);
            }
        }

        for (auto it = lhs.begin(); it != lhs.end(); ++it)
        {
            if (not (*it)->is_equality())
            {
                std::string arity = (*it)->get_predicate_arity();
                m_lhs_to_axioms[arity].insert(id);
                insert_arity(arity);
            }
        }
    }
}


void knowledge_base_t::insert_inconsistency(
    const lf::logical_function_t &func, const std::string &name)
{
    if (m_state == STATE_COMPILE)
    {
        if (not func.is_valid_as_inconsistency())
        {
            print_warning_fmt(
                "Inconsistency \"%s\" is invalid and skipped.",
                func.to_string().c_str());
        }
        else
        {
            axiom_id_t id = m_axioms.num_axioms();
            m_axioms.put(name, func);

            /* REGISTER AXIOM-ID TO MAP FOR INC */
            std::vector<const literal_t*> literals = func.get_all_literals();
            for (auto it = literals.begin(); it != literals.end(); ++it)
            {
                std::string arity = (*it)->get_predicate_arity();
                m_inc_to_axioms[arity].insert(id);
            }
        }
    }
}


void knowledge_base_t::insert_unification_postponement(
    const lf::logical_function_t &func, const std::string &name)
{
    if (m_state == STATE_COMPILE)
    {
        if (not func.is_valid_as_unification_postponement())
        {
            print_warning_fmt(
                "Unification postponement \"%s\" is invalid and skipped.",
                func.to_string().c_str());
        }
        else
        {
            axiom_id_t id = m_axioms.num_axioms();
            m_axioms.put(name, func);

            /* REGISTER AXIOM-ID TO MAP FOR UNI-PP */
            std::string arity = func.branch(0).literal().get_predicate_arity();
            if (m_arity_to_postponement.count(arity) > 0)
            {
                print_warning_fmt(
                    "The unification postponement "
                    "for the arity \"%s\" inserted redundantly!",
                    arity.c_str());
            }
            else
                m_arity_to_postponement[arity].insert(id);
        }
    }
}


void knowledge_base_t::insert_stop_word_arity(const lf::logical_function_t &f)
{
    if (m_state == STATE_COMPILE)
    {
        if (not f.is_valid_as_stop_word())
        {
            print_warning_fmt(
                "Stop-words \"%s\" is invalid and skipped.",
                f.to_string().c_str());
        }
        else
        {
            const std::vector<term_t> &terms = f.literal().terms;
            for (auto it = terms.begin(); it != terms.end(); ++it)
                m_stop_words.insert(it->string());
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


unification_postponement_t knowledge_base_t::
get_unification_postponement(const std::string &arity) const
{
    std::list<axiom_id_t> ids = search_id_list(arity, &m_cdb_uni_pp);
    if (not ids.empty())
    {
        const term_t INDISPENSABLE("*"), PARTIAL("+"), DISPENSABLE(".");
        lf::axiom_t ax = get_axiom(ids.front());
        const literal_t &lit = ax.func.branch(0).literal();
        std::string arity = lit.get_predicate_arity();
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
                    arity.c_str());
                return unification_postponement_t();
            }

        }

        int num(1);
        ax.func.param2int(&num);

        return unification_postponement_t(arity, args, num);
    }
    else
        return unification_postponement_t();
}


argument_set_id_t knowledge_base_t::
search_argument_set_id(const std::string &arity, int term_idx) const
{
    if (not m_cdb_arg_set.is_readable())
    {
        print_warning("kb-search: Kb-state is invalid.");
        return 0;
    }

    char buf[16];
    std::string key = format("%s/%d", arity.c_str(), term_idx);
    size_t value_size;
    const argument_set_id_t *value = (const argument_set_id_t*)
        m_cdb_arg_set.get(key.c_str(), key.length(), &value_size);

    return (value == NULL) ? 0 : (*value);
}


float knowledge_base_t::get_distance(
    const std::string &arity1, const std::string &arity2 ) const
{
    if (not m_cdb_rm_idx.is_readable() or not m_rm.is_readable())
    {
        print_warning(
            "get-distance: KB is currently not readable.");
        return -1;
    }

    const size_t *get1 = search_arity_index(arity1);
    const size_t *get2 = search_arity_index(arity2);
    if (get1 == NULL or get2 == NULL) return -1.0f;

    std::lock_guard<std::mutex> lock(ms_mutex_for_cache);
    auto found1 = m_cache_distance.find(*get1);
    if (found1 != m_cache_distance.end())
    {
        auto found2 = found1->second.find(*get2);
        if (found2 != found1->second.end())
            return found2->second;
    }

    float dist(m_rm.get(*get1, *get2));
    m_cache_distance[*get1][*get2] = dist;
    return dist;
}


void knowledge_base_t::_insert_cdb(
    const hash_map<std::string, hash_set<axiom_id_t> > &ids,
    cdb_data_t *dat)
{
   std::cerr
        << time_stamp() << "starts writing " << dat->filename() << "..."
        << std::endl;

    for( auto it=ids.begin(); it!=ids.end(); ++it )
    {
        size_t read_size = sizeof(size_t) + sizeof(axiom_id_t) * it->second.size();
        char *buffer = new char[read_size];

        int size = to_binary<size_t>(it->second.size(), buffer);
        for (auto id = it->second.begin(); id != it->second.end(); ++id)
            size += to_binary<axiom_id_t>(*id, buffer + size);

        assert(read_size == size);
        dat->put(it->first.c_str(), it->first.length(), buffer, size);
        delete [] buffer;
    }

    std::cerr
        << time_stamp() << "completed writing "
        << dat->filename() << "." << std::endl;
}


void knowledge_base_t::insert_arity(const std::string &arity)
{
    if (m_arity_set.count(arity) == 0)
    {
        size_t idx = m_arity_set.size();
        m_cdb_rm_idx.put(arity.c_str(), arity.length(), &idx, sizeof(size_t));
        m_arity_set.insert(arity);
    }
}


void knowledge_base_t::insert_axiom_group_to_cdb()
{
    const int SIZE(512 * 512);
    char buffer[SIZE];
    cdb_data_t &dat(m_cdb_axiom_group);
    const hash_map<std::string, hash_set<axiom_id_t> >& map(m_group_to_axioms);
    hash_map<axiom_id_t, hash_set<std::string> > axiom_to_group;

    print_console("starts writing " + dat.filename() + "...");

    for (auto it = map.begin(); it != map.end(); ++it)
    {
        size_t read_size = sizeof(size_t)+sizeof(axiom_id_t)* it->second.size();
        assert(read_size < SIZE);

        int size = to_binary<size_t>(it->second.size(), buffer);
        for (auto id = it->second.begin(); id != it->second.end(); ++id)
        {
            size += to_binary<axiom_id_t>(*id, buffer + size);
            axiom_to_group[*id].insert(it->first);
        }

        dat.put(it->first.c_str(), it->first.length(), buffer, size);
    }

    for (auto it = axiom_to_group.begin(); it != axiom_to_group.end(); ++it)
    {
        int size = to_binary<size_t>(it->second.size(), buffer);
        for (auto grp = it->second.begin(); grp != it->second.end(); ++grp)
            size += string_to_binary(*grp, buffer + size);

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


void knowledge_base_t::create_reachable_matrix()
{
    print_console("starts to create reachable matrix...");

    size_t N(m_arity_set.size()), processed(0), num_inserted(0);
    clock_t clock_past = clock_t();
    time_t time_start, time_end;
    time(&time_start);

    m_axioms.prepare_query();
    m_cdb_rhs.prepare_query();
    m_cdb_lhs.prepare_query();
    m_cdb_inc_pred.prepare_query();
    m_cdb_rm_idx.prepare_query();

    m_rm.prepare_compile();

    print_console_fmt("  num of axioms = %d", m_axioms.num_axioms());
    print_console_fmt("  num of arities = %d", N);
    print_console_fmt("  max distance = %.2f", get_max_distance());
    print_console_fmt("  num of parallel threads = %d", ms_thread_num_for_rm);

    hash_map<size_t, hash_map<size_t, float> > base_lhs, base_rhs;
    print_console("  computing distance of direct edges...");
    _create_reachable_matrix_direct(m_arity_set, &base_lhs, &base_rhs);

    print_console("  writing reachable matrix...");
    std::vector<std::thread> worker;
    int num_thread =
        std::min<int>(m_arity_set.size(),
        std::min<int>(ms_thread_num_for_rm, std::thread::hardware_concurrency()));
    
    for (int th_id = 0; th_id < num_thread; ++th_id)
    {
        worker.emplace_back(
            [&](int th_id)
            {
                for(int idx = th_id; idx < m_arity_set.size(); idx += num_thread)
                {
                    hash_map<size_t, float> dist;
                    _create_reachable_matrix_indirect(idx, base_lhs, base_rhs, &dist);
                    m_rm.put(idx, dist);

                    ms_mutex_for_rm.lock();
                    num_inserted += dist.size();
                    ++processed;

                    clock_t c = clock();
                    if (c - clock_past > CLOCKS_PER_SEC)
                    {
                        float progress = (float)(processed)* 100.0f / (float)N;
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
    
    for (size_t idx = 0; idx < m_arity_set.size(); ++idx)
    {
    }

    time(&time_end);
    int proc_time(time_end - time_start); 
    double coverage(num_inserted * 100.0 / (double)(N * N));
    
    print_console("completed computation.");
    print_console_fmt("  process-time = %d", proc_time);
    print_console_fmt("  coverage = %.6lf%%", coverage);
}


void knowledge_base_t::_create_reachable_matrix_direct(
    const hash_set<std::string> &arities,
    hash_map<size_t, hash_map<size_t, float> > *out_lhs,
    hash_map<size_t, hash_map<size_t, float> > *out_rhs)
{
    hash_set<size_t> stopped;
    for (auto it = m_stop_words.begin(); it != m_stop_words.end(); ++it)
    {
        const size_t *idx = search_arity_index(*it);
        if (idx != NULL) stopped.insert(*idx);
    }
    
    auto _process = [&](bool is_forward)
    {
        hash_map<size_t, hash_map<size_t, float> > *out(is_forward ? out_lhs : out_rhs);
        size_t num_processed(0);
        
        for (auto ar = arities.begin(); ar != arities.end(); ++ar)
        {            
            const size_t *idx1 = search_arity_index(*ar);
            assert(idx1 != NULL);
            
            if (stopped.count(*idx1) == 0)
            {
                hash_map<size_t, float> *target = &(*out)[*idx1];
                (*target)[*idx1] = 0.0f;

                std::list<axiom_id_t> ids = is_forward ?
                    search_axioms_with_lhs(*ar) :
                    search_axioms_with_rhs(*ar);

                for (auto id = ids.begin(); id != ids.end(); ++id)
                {
                    lf::axiom_t axiom = get_axiom(*id);
                    std::vector<const literal_t*>
                        lits = axiom.func.branch(is_forward ? 1 : 0).get_all_literals();
                    float dist = (*m_rm_dist)(axiom);
                    
                    if (dist < 0.0f) continue;

                    for (auto li = lits.begin(); li != lits.end(); ++li)
                    {
                        std::string arity2 = (*li)->get_predicate_arity();
                        const size_t *idx2 = search_arity_index(arity2);
                    
                        if (idx2 == NULL) continue;
                        if (stopped.count(*idx2) != 0) continue;

                        auto found = target->find(*idx2);
                        if (found == target->end())    (*target)[*idx2] = dist;
                        else if (dist < found->second) (*target)[*idx2] = dist;
                    }
                }
            }

            if (++num_processed % 10 == 0)
            {
                float progress = (float)(num_processed) * 100.0f / (float)arities.size();
                std::cerr << format("processed %d predicates [%.4f%%]\r", num_processed, progress);
            }
                    
        }
    };

    _process(true);
    _process(false);
}


void knowledge_base_t::_create_reachable_matrix_indirect(
    size_t idx1,
    const hash_map<size_t, hash_map<size_t, float> > &base_lhs,
    const hash_map<size_t, hash_map<size_t, float> > &base_rhs,
    hash_map<size_t, float> *out) const
{
    if (base_lhs.count(idx1) == 0 or base_rhs.count(idx1) == 0) return;

    std::map<std::tuple<size_t, bool, bool>, float> current;
    std::map<std::tuple<size_t, bool, bool>, float> processed;

    current[std::make_tuple(idx1, true, true)] = 0.0f;
    processed[std::make_tuple(idx1, true, true)] = 0.0f;
    (*out)[idx1] = 0.0f;

    while (not current.empty())
    {
        std::map<std::tuple<size_t, bool, bool>, float> next;
        auto _process = [&](
            size_t idx, bool can_abduction, bool can_deduction,
            float dist, bool is_forward)
        {
            const hash_map<size_t, hash_map<size_t, float> >
                &base = (is_forward ? base_lhs : base_rhs);
            auto found = base.find(idx);

            if (found != base.end())
            for (auto it2 = found->second.begin(); it2 != found->second.end(); ++it2)
            {
                size_t idx2(it2->first);
                float dist_new(dist + it2->second); // DISTANCE idx1 ~ idx2

                if (idx == idx2) continue;

                if (get_max_distance() < 0.0f or dist_new <= get_max_distance())
                {
                    std::tuple<size_t, bool, bool> key =
                        std::make_tuple(idx2, can_abduction, can_deduction);
                    if (is_forward and not ms_do_compute_distance_for_deduction)
                        std::get<1>(key) = false;
                    if (not is_forward and not ms_do_compute_distance_for_abduction)
                        std::get<2>(key) = false;

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
            size_t idx = std::get<0>(it1->first);
            bool can_abduction = std::get<1>(it1->first);
            bool can_deduction = std::get<2>(it1->first);

            if (can_abduction)
                _process(idx, can_abduction, can_deduction, it1->second, false);
            if (can_deduction)
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


void knowledge_base_t::set_distance_provider(distance_provider_type_e t)
{
    distance_provider_t *ptr = NULL;
    switch (t)
    {
    case DISTANCE_PROVIDER_BASIC:
        ptr = new basic_distance_provider_t(); break;
    case DISTANCE_PROVIDER_COST_BASED:
        ptr = new cost_based_distance_provider_t(); break;
    }

    if (ptr != NULL)
    {
        delete m_rm_dist;
        m_rm_dist = ptr;
    }
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


float cost_based_distance_provider_t::operator()(const lf::axiom_t &ax) const
{
    const std::string &param = ax.func.param();
    float out(-1.0f);
    _sscanf(param.substr(1).c_str(), "%f", &out);
    return out;
}


} // end kb

} // end phil

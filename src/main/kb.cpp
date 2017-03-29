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
#include "./binary.h"


namespace dav
{

namespace kb
{


const axiom_id_t INVALID_AXIOM_ID = -1;
const argument_set_id_t INVALID_ARGUMENT_SET_ID = 0;
const predicate_id_t INVALID_PREDICATE_ID = 0;
const predicate_id_t EQ_PREDICATE_ID = 1;




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
      axioms(filename),
      predicates(filename + ".arity.dat"),
      heuristics(filename + ".rm.dat")
{
    m_distance_provider = { NULL, "" };

    m_config_for_compile.max_distance = ph->param_float("kb-max-distance", -1.0);
    m_config_for_compile.thread_num = ph->param_int("kb-thread-num", 1);
    m_config_for_compile.do_disable_stop_word = ph->flag("disable-stop_word");
    m_config_for_compile.can_deduction = ph->flag("enable-deduction");
    m_config_for_compile.do_print_heuristics = ph->flag("print-reachability-matrix");

    if (m_config_for_compile.thread_num < 1)
        m_config_for_compile.thread_num = 1;

    std::string dist_key = ph->param("distance-provider");
    set_distance_provider(dist_key.empty() ? "basic" : dist_key, ph);
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

    if (m_state == STATE_QUERY)
        finalize();

    if (m_state == STATE_NULL)
    {
        axioms.prepare_compile();


        m_state = STATE_COMPILE;
    }
}


void knowledge_base_t::prepare_query()
{
    if (m_distance_provider.instance == NULL)
        throw phillip_exception_t(
        "Preparing KB had failed, "
        "because distance provider has not been set.");

    if (m_state == STATE_COMPILE)
        finalize();

    if (m_state == STATE_NULL)
    {
        read_config();
        predicates.load();
        axioms.prepare_query();

        heuristics.prepare_query();

        m_state = STATE_QUERY;
    }
}


void knowledge_base_t::finalize()
{
    if (m_state == STATE_NULL) return;

    kb_state_e state = m_state;
    m_state = STATE_NULL;

    axioms.finalize();
    heuristics.finalize();

    if (state == STATE_COMPILE)
    {
        extend_inconsistency();
        create_reachable_matrix();
        write_config();
        predicates.write();

        if (m_config_for_compile.do_print_heuristics)
        {
            std::cerr << "Reachability Matrix:" << std::endl;
            heuristics.prepare_query();

            std::cerr << std::setw(30) << std::right << "" << " | ";
            for (auto arity : predicates.arities())
                std::cerr << arity << " | ";
            std::cerr << std::endl;

            for (auto a1 : predicates.arities())
            {
                predicate_id_t idx1 = predicates.pred2id(a1);
                std::cerr << std::setw(30) << std::right << a1 << " | ";

                for (auto a2 : predicates.arities())
                {
                    predicate_id_t idx2 = predicates.pred2id(a2);
                    float dist = heuristics.get(idx1, idx2);
                    std::cerr << std::setw(a2.length()) << dist << " | ";
                }
                std::cerr << std::endl;
            }
        }

        predicates.clear();
    }
}


void knowledge_base_t::write_config() const
{
    std::string filename(m_filename + ".conf");
    std::ofstream fo(
        filename.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
    char version(NUM_OF_KB_VERSION_TYPES - 1); // LATEST VERSION
    char num_dp = m_distance_provider.key.length();

    if (not fo)
        throw phillip_exception_t(
        util::format("Cannot open KB-configuration file: \"%s\"", filename.c_str()));

    fo.write(&version, sizeof(char));
    fo.write((char*)&m_config_for_compile.max_distance, sizeof(float));

    fo.write(&num_dp, sizeof(char));
    fo.write(m_distance_provider.key.c_str(), m_distance_provider.key.length());
    m_distance_provider.instance->write(&fo);

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
        m_version = KB_VERSION_UNSPECIFIED;
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

    fi.close();
}


std::list<conjunction_pattern_t> knowledge_base_t::axioms_database_t::patterns(predicate_id_t pid) const
{
    std::list<conjunction_pattern_t> out;

    if (not m_cdb_arity_patterns.is_readable())
    {
        util::print_warning("kb-search: Kb-state is invalid.");
        return out;
    }

    size_t value_size;
    const char *value = (const char*)m_cdb_arity_patterns.get(&pid, sizeof(predicate_id_t), &value_size);

    if (value != nullptr)
    {
        size_t num_query, read_size(0);
        read_size += util::binary_to<size_t>(value, &num_query);
        out.assign(num_query, conjunction_pattern_t());

        for (auto &p : out)
            read_size += binary_to_pattern(value + read_size, &p);
    }

    return out;
}


std::list<std::pair<axiom_id_t, bool>> knowledge_base_t::axioms_database_t::
gets_by_pattern(const conjunction_pattern_t &pattern) const
{
    std::list<std::pair<axiom_id_t, is_backward_t>> out;

    if (not m_cdb_pattern_to_ids.is_readable())
    {
        util::print_warning("kb-search: Kb-state is invalid.");
        return out;
    }

    std::vector<char> key;
    pattern_to_binary(pattern, &key);

    size_t value_size;
    const char *value = (const char*)
        m_cdb_pattern_to_ids.get(&key[0], key.size(), &value_size);

    if (value != nullptr)
    {
        size_t size(0), num_id(0);
        size += util::binary_to<size_t>(value + size, &num_id);
        out.assign(num_id, std::pair<axiom_id_t, is_backward_t>());

        for (auto &p : out)
        {
            char flag;
            size += util::binary_to<axiom_id_t>(value + size, &(p.first));
            size += util::binary_to<char>(value + size, &flag);
            p.second = (flag != 0x00);
        }
    }

    return out;
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


float knowledge_base_t::get_distance(
    const std::string &arity1, const std::string &arity2 ) const
{
    predicate_id_t get1 = predicates.pred2id(arity1);
    predicate_id_t get2 = predicates.pred2id(arity2);
    return get_distance(get1, get2);
}


float knowledge_base_t::get_distance(predicate_id_t a1, predicate_id_t a2) const
{
    if (a1 == INVALID_PREDICATE_ID or a2 == INVALID_PREDICATE_ID)
        return -1.0f;

    std::lock_guard<std::mutex> lock(ms_mutex_for_cache);
    auto found1 = m_cache_distance.find(a1);
    if (found1 != m_cache_distance.end())
    {
        auto found2 = found1->second.find(a2);
        if (found2 != found1->second.end())
            return found2->second;
    }

    float dist(heuristics.get(a1, a2));
    m_cache_distance[a1][a2] = dist;
    return dist;
}


void knowledge_base_t::create_reachable_matrix()
{
    IF_VERBOSE_1("starts to create reachable matrix...");

    size_t processed(0), num_inserted(0);
    clock_t clock_past = clock_t();
    time_t time_start, time_end;
    time(&time_start);

    axioms.prepare_query();
    heuristics.prepare_compile();

    IF_VERBOSE_3(util::format("  num of axioms = %d", axioms.size()));
    IF_VERBOSE_3(util::format("  num of arities = %d", predicates.arities().size()));
    IF_VERBOSE_3(util::format("  max distance = %.2f", get_max_distance()));
    IF_VERBOSE_3(util::format("  num of parallel threads = %d", m_config_for_compile.thread_num));
    IF_VERBOSE_2("  computing distance of direct edges...");

    const std::vector<predicate_with_arity_t> &arities = predicates.arities();
    hash_map<predicate_id_t, hash_map<predicate_id_t, float> > base_lhs, base_rhs;
    hash_set<predicate_id_t> ignored;
    std::set<std::pair<predicate_id_t, predicate_id_t> > base_para;
    
    for (const auto &p : predicates.functional_predicates())
        ignored.insert(p.first);
    ignored.insert(INVALID_PREDICATE_ID);
    
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
                for(predicate_id_t idx = th_id; idx < arities.size(); idx += num_thread)
                {
                    if (ignored.count(idx) != 0) continue;
                    
                    hash_map<predicate_id_t, float> dist;
                    _create_reachable_matrix_indirect(
                        idx, base_lhs, base_rhs, base_para, &dist);
                    heuristics.put(idx, dist);

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
    double coverage(num_inserted * 100.0 / (double)(arities.size() * arities.size()));
    
    IF_VERBOSE_1("completed computation.");
    IF_VERBOSE_3(util::format("  process-time = %d", proc_time));
    IF_VERBOSE_3(util::format("  coverage = %.6lf%%", coverage));
}


void knowledge_base_t::_create_reachable_matrix_direct(
    const hash_set<predicate_id_t> &ignored,
    hash_map<predicate_id_t, hash_map<predicate_id_t, float> > *out_lhs,
    hash_map<predicate_id_t, hash_map<predicate_id_t, float> > *out_rhs,
    std::set<std::pair<predicate_id_t, predicate_id_t> > *out_para)
{
    size_t num_processed(0);
    const std::vector<predicate_with_arity_t> &arities = predicates.arities();

    for (predicate_id_t i = 1; i < arities.size(); ++i)
    {
        if (ignored.count(i) == 0)
        {
            (*out_lhs)[i][i] = 0.0f;
            (*out_rhs)[i][i] = 0.0f;
        }
    }

    for (axiom_id_t id = 0; id < axioms.size(); ++id)
    {
        if (++num_processed % 10 == 0 and phillip_main_t::verbose() >= VERBOSE_1)
        {
            float progress = (float)(num_processed)* 100.0f / (float)axioms.size();
            std::cerr << util::format("processed %d axioms [%.4f%%]\r", num_processed, progress);
        }

        lf::axiom_t axiom = axioms.get(id);
        if (not axiom.func.is_operator(lf::OPR_IMPLICATION)) continue;

        float dist = (*m_distance_provider.instance)(axiom);
        if (dist >= 0.0f)
        {
            hash_set<predicate_id_t> lhs_ids, rhs_ids;

            {
                std::vector<const literal_t*> lhs = axiom.func.get_lhs();
                std::vector<const literal_t*> rhs = axiom.func.get_rhs();

                for (auto it_l = lhs.begin(); it_l != lhs.end(); ++it_l)
                {
                    std::string arity = (*it_l)->predicate_with_arity();
                    predicate_id_t idx = predicates.pred2id(arity);

                    if (idx != INVALID_PREDICATE_ID)
                    if (ignored.count(idx) == 0)
                        lhs_ids.insert(idx);
                }

                for (auto it_r = rhs.begin(); it_r != rhs.end(); ++it_r)
                {
                    std::string arity = (*it_r)->predicate_with_arity();
                    predicate_id_t idx = predicates.pred2id(arity);

                    if (idx != INVALID_PREDICATE_ID)
                    if (ignored.count(idx) == 0)
                        rhs_ids.insert(idx);
                }
            }

            for (auto it_l = lhs_ids.begin(); it_l != lhs_ids.end(); ++it_l)
            {
                hash_map<predicate_id_t, float> &target = (*out_lhs)[*it_l];
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
                hash_map<predicate_id_t, float> &target = (*out_rhs)[*it_r];
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
    predicate_id_t target,
    const hash_map<predicate_id_t, hash_map<predicate_id_t, float> > &base_lhs,
    const hash_map<predicate_id_t, hash_map<predicate_id_t, float> > &base_rhs,
    const std::set<std::pair<predicate_id_t, predicate_id_t> > &base_para,
    hash_map<predicate_id_t, float> *out) const
{
    if (base_lhs.count(target) == 0 or base_rhs.count(target) == 0) return;

    std::map<std::tuple<predicate_id_t, bool, bool>, float> processed;

    std::function<void(predicate_id_t, bool, bool, float, bool)> process = [&](
        predicate_id_t idx1, bool can_abduction, bool can_deduction, float dist, bool is_forward)
    {
        const hash_map<predicate_id_t, hash_map<predicate_id_t, float> >
            &base = (is_forward ? base_lhs : base_rhs);
        auto found = base.find(idx1);

        if (found != base.end())
        for (auto it2 = found->second.begin(); it2 != found->second.end(); ++it2)
        {
            if (idx1 == it2->first) continue;
            if (it2->second < 0.0f) continue;

            predicate_id_t idx2(it2->first);
            bool is_paraphrasal =
                (base_para.count(util::make_sorted_pair(idx1, idx2)) > 0);
            if (not is_paraphrasal and
                ((is_forward and not can_deduction) or
                (not is_forward and not can_abduction)))
                continue;

            float dist_new(dist + it2->second); // DISTANCE idx1 ~ idx2
            if (get_max_distance() >= 0.0f and dist_new > get_max_distance())
                continue;

            std::tuple<predicate_id_t, bool, bool> key =
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
            lf::axiom_t axiom = axioms.get(*ax);
            auto literals = axiom.func.get_all_literals();
            if (literals.size() != 2) continue;
        }
    }
#endif
}


std::mutex knowledge_base_t::axioms_database_t::ms_mutex;

knowledge_base_t::axioms_database_t::axioms_database_t(const std::string &filename)
: m_filename(filename),
m_num_compiled_axioms(0), m_num_unnamed_axioms(0),
m_cdb_rhs(filename + ".rhs.cdb"),
m_cdb_lhs(filename + ".lhs.cdb"),
m_cdb_arity_patterns(filename + ".pattern.cdb"),
m_cdb_pattern_to_ids(filename + ".search.cdb")
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

        m_fo_idx.reset(new std::ofstream(
            (m_filename + ".index.dat").c_str(), std::ios::binary | std::ios::out));
        m_fo_dat.reset(new std::ofstream(
            (m_filename + ".axioms.dat").c_str(), std::ios::binary | std::ios::out));
        m_num_compiled_axioms = 0;
        m_num_unnamed_axioms = 0;
        m_writing_pos = 0;

        m_cdb_rhs.prepare_compile();
        m_cdb_lhs.prepare_compile();
        m_cdb_arity_patterns.prepare_compile();
        m_cdb_pattern_to_ids.prepare_compile();
    }
}


void knowledge_base_t::axioms_database_t::prepare_query()
{
    auto read_groups = [this](const std::string &filename)
    {
        std::ifstream fi(filename, std::ios::binary | std::ios::in);

        size_t size;
        fi.read((char*)&size, sizeof(size_t));

        for (size_t i = 0; i < size; ++i)
        {
            m_groups.groups.push_back(hash_set<axiom_id_t>());
            hash_set<axiom_id_t> &grp = m_groups.groups.back();

            size_t n;
            fi.read((char*)&n, sizeof(size_t));

            for (size_t j = 0; j < n; ++j)
            {
                axiom_id_t id;
                fi.read((char*)&id, sizeof(axiom_id_t));
                grp.insert(id);
                m_groups.ax2gr[id].push_back(&grp);
            }
        }
    };

    if (is_writable())
        finalize();

    if (not is_readable())
    {
        std::lock_guard<std::mutex> lock(ms_mutex);

        m_fi_idx.reset(new std::ifstream(
            (m_filename + ".index.dat").c_str(), std::ios::binary | std::ios::in));
        m_fi_dat.reset(new std::ifstream(
            (m_filename + ".axioms.dat").c_str(), std::ios::binary | std::ios::in));

        m_fi_idx->seekg(-static_cast<int>(sizeof(int)), std::ios_base::end);
        m_fi_idx->read((char*)&m_num_compiled_axioms, sizeof(int));

        read_groups(m_filename + ".grp");

        m_cdb_rhs.prepare_query();
        m_cdb_lhs.prepare_query();
        m_cdb_arity_patterns.prepare_query();
        m_cdb_pattern_to_ids.prepare_query();
    }
}


void knowledge_base_t::axioms_database_t::finalize()
{
    auto write_groups = [this](const std::string &filename)
    {
        std::ofstream fo(filename, std::ios::binary | std::ios::trunc | std::ios::out);

        IF_VERBOSE_1("Writing " + filename + "...");

        size_t size = m_tmp.gr2ax.size();
        fo.write((char*)&size, sizeof(size_t));

        for (auto p : m_tmp.gr2ax)
        {
            size_t n = p.second.size();
            fo.write((char*)&n, sizeof(size_t));

            for (auto a : p.second)
                fo.write((char*)&a, sizeof(axiom_id_t));
        }
    };

    auto write_map = [](
        const hash_map<predicate_id_t, hash_set<axiom_id_t> > &ids,
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
            dat->put((char*)&it->first, sizeof(predicate_id_t), buffer, size);
            delete[] buffer;
        }

        IF_VERBOSE_1("completed writing " + dat->filename() + ".");
    };

    auto write_patterns = [this]()
    {
        IF_VERBOSE_1("Writing the conjunction patterns...");
        IF_VERBOSE_2("    -> Writing \"" + m_cdb_arity_patterns.filename() + "\"...");

        for (auto p : m_tmp.pred2pats)
        {
            std::list< std::vector<char> > patterns; // PATTERNS WRITEN IN BINARY
            size_t size_value(0);

            size_value += sizeof(size_t);
            for (auto q : p.second)
            {
                std::vector<char> bin;
                pattern_to_binary(q, &bin);
                patterns.push_back(bin);
                size_value += bin.size();
            }

            char *value = new char[size_value];
            size_t size(0);

            size += util::to_binary<size_t>(p.second.size(), value);
            for (auto p : patterns)
            {
                std::memcpy(value + size, &p[0], p.size());
                size += p.size();
            }

            assert(size == size_value);
            m_cdb_arity_patterns.put((char*)(&p.first), sizeof(predicate_id_t), value, size_value);

            delete[] value;
        }

        IF_VERBOSE_2("    -> Writing \"" + m_cdb_pattern_to_ids.filename() + "\"...");
        IF_VERBOSE_3(util::format("        # of patterns = %d", m_tmp.pat2ax.size()));

        for (auto p : m_tmp.pat2ax)
        {
            std::vector<char> key, val;
            pattern_to_binary(p.first, &key);

            size_t size_val = sizeof(size_t)+(sizeof(axiom_id_t)+sizeof(char)) * p.second.size();
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
    };

    if (is_writable())
    {
        m_fo_idx->write((char*)&m_num_compiled_axioms, sizeof(int));
        m_fo_idx.reset();
        m_fo_dat.reset();

        write_groups(m_filename + ".grp");
        write_map(m_tmp.rhs2ax, &m_cdb_rhs);
        write_map(m_tmp.lhs2ax, &m_cdb_lhs);
        write_patterns();
    }

    m_fi_idx.reset();
    m_fi_dat.reset();

    m_cdb_rhs.finalize();
    m_cdb_lhs.finalize();
    m_cdb_arity_patterns.finalize();
    m_cdb_pattern_to_ids.finalize();

    m_tmp.rhs2ax.clear();
    m_tmp.lhs2ax.clear();
    m_tmp.gr2ax.clear();
}


axiom_id_t knowledge_base_t::axioms_database_t::add(
    const lf::logical_function_t &func, const string_t &name)
{
    auto map_pattern = [&func, this](
        const std::vector<const literal_t*> &conj, axiom_id_t ax_id, bool is_backward)
    {
        hash_map<string_hash_t, std::set<std::pair<predicate_id_t, term_idx_t> > > term2arity;
        conjunction_pattern_t pattern;

        for (index_t i = 0; i < conj.size(); ++i)
        {
            const literal_t &lit = (*conj[i]);
            std::string arity = lit.predicate_with_arity();
            predicate_id_t idx = kb::kb()->predicates.pred2id(arity);

            assert(idx != INVALID_PREDICATE_ID);
            pattern.predicates().push_back(idx);

            for (term_idx_t j = 0; j < lit.terms().size(); ++j)
            if (lit.terms().at(j).is_hard_term())
                term2arity[lit.terms().at(j)].insert(std::make_pair(i, j));
        }

        // ENUMERATE HARD TERMS
        for (auto e : term2arity)
        {
            for (auto it1 = e.second.begin(); it1 != e.second.end(); ++it1)
            for (auto it2 = e.second.begin(); it2 != it1; ++it2)
                pattern.hardterms().push_back(util::make_sorted_pair(*it1, *it2));
        }
        pattern.hardterms().sort();

        m_tmp.pat2ax[pattern].insert(std::make_pair(ax_id, is_backward));

        // NON-FUNCTIONAL PREDICATES ARE ASSIGNED TO THE PATTERN
        for (auto pid : pattern.predicates())
        if (not kb::kb()->predicates.is_functional(pid))
            m_tmp.pred2pats[pid].insert(pattern);
    };

    if (not is_writable())
    {
        util::print_warning_fmt(
            "SKIPPED: \"%s\"\n"
            "    -> The knowledge base is not writable.", func.repr().c_str());
        return INVALID_AXIOM_ID;
    }

    if (not func.is_valid_as_implication())
    {
        util::print_warning_fmt(
            "SKIPPED: \"%s\"\n"
            "    -> This axiom is invalid.", func.repr().c_str());
        return INVALID_AXIOM_ID;
    }

    // REGISTER PREDICATES IN func TO KNOWLEDGE-BASE.
    std::vector<const lf::logical_function_t*> branches;
    func.enumerate_literal_branches(&branches);
    for (const auto &br : branches)
        kb()->predicates.add(br->literal().predicate_with_arity());

    // WRITE AXIOM TO KNOWLEDGE-BASE.
    axiom_id_t id = size();
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

    // REGISTER AXIOMS'S GROUPS
    std::vector<group_name_t> &&spl = name.split("#");
    if (spl.size() > 1)
    for (auto it = spl.begin(); std::next(it) != spl.end(); ++it)
        m_tmp.gr2ax[*it].insert(id);

    // ABDUCTION
    {
        std::vector<const literal_t*> rhs(func.get_rhs());
        for (auto p : rhs)
        if (not p->is_equality())
        {
            predicate_id_t predicate_id = kb()->predicates.pred2id(p->predicate_with_arity());
            m_tmp.rhs2ax[predicate_id].insert(id);
        }
        map_pattern(rhs, id, true);
    }

    // DEDUCTION
    if (kb()->can_deduce())
    {
        std::vector<const literal_t*> lhs(func.get_lhs());
        for (auto p : lhs)
        if (not p->is_equality())
        {
            predicate_id_t predicate_id = kb()->predicates.pred2id(p->predicate_with_arity());
            m_tmp.lhs2ax[predicate_id].insert(id);
        }
        map_pattern(lhs, id, false);
    }

    IF_VERBOSE_FULL(util::format("ADDED IMPLICATION-RULE: %s", func.repr()));

    return id;
}


lf::axiom_t knowledge_base_t::axioms_database_t::get(axiom_id_t id) const
{
    if (id >= size())
        throw phillip_exception_t(
        util::format("axioms_database_t::get: Invalid argument \"%d\" was given.", id));

    std::lock_guard<std::mutex> lock(ms_mutex);
    lf::axiom_t out;

    if (not is_readable())
    {
        util::print_warning("kb-search: KB is currently not readable.");
        return out;
    }

    axiom_pos_t ax_pos;
    axiom_size_t ax_size;
    const int SIZE(512 * 512);
    char buffer[SIZE];

    // GET THE SIZE AND THE POSITION IN m_fi_dat OF THE AXIOM FROM m_fi_idx.
    m_fi_idx->seekg(id * (sizeof(axiom_pos_t)+sizeof(axiom_size_t)));
    m_fi_idx->read((char*)&ax_pos, sizeof(axiom_pos_t));
    m_fi_idx->read((char*)&ax_size, sizeof(axiom_size_t));

    // GET THE AXIOM FROM m_fi_dat
    m_fi_dat->seekg(ax_pos);
    m_fi_dat->read(buffer, ax_size);

    out.id = id;
    size_t read_size = out.func.read_binary(buffer);
    read_size += util::binary_to_string(buffer + read_size, &out.name);

    return out;
}


hash_set<axiom_id_t> knowledge_base_t::axioms_database_t::gets_in_same_group_as(axiom_id_t id) const
{
    hash_set<axiom_id_t> out{ id };
    auto found = m_groups.ax2gr.find(id);

    if (found != m_groups.ax2gr.end())
    for (auto grp : found->second)
        out.insert(grp->begin(), grp->end());

    return out;
}


std::list<axiom_id_t> knowledge_base_t::axioms_database_t::
gets_from_cdb(const char *key, size_t key_size, const util::cdb_data_t *dat) const
{
    std::list<axiom_id_t> out;

    if (dat != nullptr)
    {
        if (not dat->is_readable())
            util::print_warning("kb-search: Kb-state is invalid.");
        else
        {
            size_t value_size;
            const char *value = (const char*)
                dat->get(key, key_size, &value_size);

            if (value != nullptr)
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




std::mutex knowledge_base_t::reachable_matrix_t::ms_mutex;


knowledge_base_t::reachable_matrix_t::reachable_matrix_t(const std::string &filename)
    : m_filename(filename)
{}


void knowledge_base_t::reachable_matrix_t::prepare_compile()
{
    if (is_readable())
        finalize();

    if (not is_writable())
    {
        std::lock_guard<std::mutex> lock(ms_mutex);
        pos_t pos;
        
        m_fout.reset(new std::ofstream(
            m_filename.c_str(), std::ios::binary | std::ios::out));
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

        m_fin.reset(new std::ifstream(
            m_filename.c_str(), std::ios::binary | std::ios::in));

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
    if (m_fout)
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

        m_fout.reset();
    }

    if (m_fin)
        m_fin.reset();

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

float null_distance_provider_t::operator()(const lf::axiom_t &ax) const
{
    return 0.0f;
}


float basic_distance_provider_t::operator()(const lf::axiom_t &ax) const
{
    float out;
    return 1.0f;
}


float cost_based_distance_provider_t::operator()(const lf::axiom_t &ax) const
{
    float out(-1.0f);
    return ax.func.scan_parameter("%f", &out) ? out : 1.0f;
}


distance_function_t* sum_of_left_hand_side_distance_provider_t::
generator_t::operator()(const phillip_main_t *ph) const
{
    return new sum_of_left_hand_side_distance_provider_t(
        ((ph == NULL) ? 1.0f : ph->param_float("default-distance", 1.0f)));
}


void sum_of_left_hand_side_distance_provider_t::read(std::ifstream *fi)
{
    fi->read((char*)&m_default_distance, sizeof(float));
}


void sum_of_left_hand_side_distance_provider_t::write(std::ofstream *fo) const
{
    fo->write((char*)&m_default_distance, sizeof(float));
}


float sum_of_left_hand_side_distance_provider_t::operator()(const lf::axiom_t &ax) const
{
    float out(0.0f);
    std::vector<const lf::logical_function_t*> lhs;

    ax.func.branch(0).enumerate_literal_branches(&lhs);
    
    for (auto l : lhs)
    {
        float d(m_default_distance);
        l->scan_parameter("%f", &d);
        out += d;
    }
    
    return out;
}


}


void pattern_to_binary(const conjunction_pattern_t &q, std::vector<char> *bin)
{
    size_t size_expected =
        sizeof(unsigned char) * 2 +
        sizeof(predicate_id_t) * q.predicates().size() +
        (sizeof(predicate_id_t) + sizeof(char)) * 2 * q.hardterms().size();
    size_t size(0);
    bin->assign(size_expected, '\0');

    size += util::num_to_binary(q.predicates().size(), &(*bin)[0]);
    for (auto id : q.predicates())
        size += util::to_binary<predicate_id_t>(id, &(*bin)[0] + size);

    size += util::num_to_binary(q.hardterms().size(), &(*bin)[0] + size);
    for (auto ht : q.hardterms())
    {
        size += util::to_binary<predicate_id_t>(ht.first.first, &(*bin)[0] + size);
        size += util::to_binary<char>(ht.first.second, &(*bin)[0] + size);
        size += util::to_binary<predicate_id_t>(ht.second.first, &(*bin)[0] + size);
        size += util::to_binary<char>(ht.second.second, &(*bin)[0] + size);
    }

    assert(size == size_expected);
};


size_t binary_to_pattern(const char *bin, conjunction_pattern_t *out)
{
    size_t size(0);
    int num_arity, num_hardterm, num_option;

    out->predicates().clear();
    out->hardterms().clear();

    size += util::binary_to_num(bin + size, &num_arity);
    for (int i = 0; i < num_arity; ++i)
    {
        predicate_id_t id;
        size += util::binary_to<predicate_id_t>(bin + size, &id);
        out->predicates().push_back(id);
    }

    size += util::binary_to_num(bin + size, &num_hardterm);
    for (int i = 0; i < num_hardterm; ++i)
    {
        predicate_id_t id1, id2;
        char idx1, idx2;
        size += util::binary_to<predicate_id_t>(bin + size, &id1);
        size += util::binary_to<char>(bin + size, &idx1);
        size += util::binary_to<predicate_id_t>(bin + size, &id2);
        size += util::binary_to<char>(bin + size, &idx2);
        out->hardterms().push_back(std::make_pair(
            std::make_pair(id1, idx1), std::make_pair(id2, idx2)));
    }

    return size;
}


} // end kb

} // end phil

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


configuration_t::configuration_t()
	: max_distance(-1.0), thread_num(1)
{}


const int BUFFER_SIZE = 512 * 512;
std::unique_ptr<knowledge_base_t, deleter_t<knowledge_base_t>> knowledge_base_t::ms_instance;
std::mutex knowledge_base_t::ms_mutex_for_cache;
std::mutex knowledge_base_t::ms_mutex_for_rm;


knowledge_base_t* knowledge_base_t::instance()
{
    if (not ms_instance)
        throw phillip_exception_t("An instance of knowledge-base has not been initialized.");

    return ms_instance.get();
}


void knowledge_base_t::initialize(const configuration_t &conf)
{
    if (ms_instance != NULL)
        ms_instance.reset(NULL);
        
	conf.path.dirname().mkdir();
    ms_instance.reset(new knowledge_base_t(conf));

	predicate_library_t::instance()->filepath() = conf.path + ".pred.dat";
}


knowledge_base_t::knowledge_base_t(const configuration_t &conf)
    : m_state(STATE_NULL),
	m_version(KB_VERSION_1), 
	axioms(conf.path),
	m_config(conf),
	heuristics(conf.path + ".rm.dat")
{
    m_distance_provider = { NULL, "" };

    auto dist_key = param()->get("distance-provider");
    set_distance_provider(dist_key.empty() ? "basic" : conf.dp_key);
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
        predicate_library_t::instance()->load();
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

	auto *preds = predicate_library_t::instance();

    if (state == STATE_COMPILE)
    {
        extend_inconsistency();
        create_reachable_matrix();
        write_config();
        preds->write();

        if (param()->has("print-reachability"))
        {
			std::ofstream fo(param()->get("print-reachability"));

            fo << "Reachability Matrix:" << std::endl;
            heuristics.prepare_query();

            fo << std::setw(30) << std::right << "" << " | ";
            for (auto p : preds->predicates())
                fo << p.string() << " | ";
            fo << std::endl;

            for (auto p1 : preds->predicates())
            {
                predicate_id_t idx1 = preds->pred2id(p1);
                fo << std::setw(30) << std::right << p1.string() << " | ";

                for (auto p2 : preds->predicates())
                {
                    predicate_id_t idx2 = preds->pred2id(p2);
                    float dist = heuristics.get(idx1, idx2);
                    fo << std::setw(p2.string().length()) << dist << " | ";
                }
                fo << std::endl;
            }
        }
    }
}


void knowledge_base_t::write_config() const
{
    std::string filename(m_config.path + ".conf");
    std::ofstream fo(
        filename.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
    char version(NUM_OF_KB_VERSION_TYPES - 1); // LATEST VERSION
    char num_dp = m_distance_provider.key.length();

    if (not fo)
        throw phillip_exception_t(
        format("Cannot open KB-configuration file: \"%s\"", filename.c_str()));

    fo.write(&version, sizeof(char));
    fo.write((char*)&m_config.max_distance, sizeof(float));

    fo.write(&num_dp, sizeof(char));
    fo.write(m_distance_provider.key.c_str(), m_distance_provider.key.length());

    fo.close();
}


void knowledge_base_t::read_config()
{
    std::string filename(m_config.path + ".conf");
    std::ifstream fi(filename.c_str(), std::ios::in | std::ios::binary);
    char version, num;
    char key[256];

    if (not fi)
        throw phillip_exception_t(
        format("Cannot open KB-configuration file: \"%s\"", filename.c_str()));

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

    fi.read((char*)&m_config.max_distance, sizeof(float));

    fi.read(&num, sizeof(char));
    fi.read(key, num);
    key[num] = '\0';
    set_distance_provider(key);

    fi.close();
}


std::list<conjunction_pattern_t> knowledge_base_t::rule_library_t::patterns(predicate_id_t pid) const
{
    std::list<conjunction_pattern_t> out;

    if (not m_cdb_arity_patterns.is_readable())
    {
        console()->warn("kb-search: Kb-state is invalid.");
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


std::list<std::pair<axiom_id_t, bool>> knowledge_base_t::rule_library_t::
gets_by_pattern(const conjunction_pattern_t &pattern) const
{
    std::list<std::pair<axiom_id_t, is_right_hand_side_t>> out;

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
        out.assign(num_id, std::pair<axiom_id_t, is_right_hand_side_t>());

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


} // end kb

} // end phil

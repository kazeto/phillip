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
#include "./binary.h"


namespace dav
{

namespace kb
{


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


void knowledge_base_t::initialize(const filepath_t &path)
{
	assert(not ms_instance);
        
	path.dirname().mkdir();
    ms_instance.reset(new knowledge_base_t(path));

	predicate_library_t::instance()->filepath() = path + ".pred.dat";
}


knowledge_base_t::knowledge_base_t(const filepath_t &path)
    : m_state(STATE_NULL), m_version(KB_VERSION_1),	m_path(path),
	rules(path + "base.dat"),
	lhs2rids(path + ".lhs.cdb"),
	rhs2rids(path + ".rhs.cdb"),
	class2rids(path + ".cls.cdb"),
	feat2rids(path + ".ft.cdb")
{}


knowledge_base_t::~knowledge_base_t()
{
    finalize();
}


void knowledge_base_t::prepare_compile()
{
    if (m_state == STATE_QUERY)
        finalize();

    if (m_state == STATE_NULL)
    {
		rules.prepare_compile();
		lhs2rids.prepare_compile();
		rhs2rids.prepare_compile();
		class2rids.prepare_compile();
		feat2rids.prepare_compile();

        m_state = STATE_COMPILE;
    }
}


void knowledge_base_t::prepare_query()
{
    if (m_state == STATE_COMPILE)
        finalize();

    if (m_state == STATE_NULL)
    {
		rules.prepare_query();
		lhs2rids.prepare_query();
		rhs2rids.prepare_query();
		class2rids.prepare_query();
		feat2rids.prepare_query();

        m_state = STATE_QUERY;
    }
}


void knowledge_base_t::finalize()
{
    if (m_state == STATE_NULL) return;

    kb_state_e state = m_state;
    m_state = STATE_NULL;

	write_spec(m_path + ".spec.txt");

	rules.finalize();
	lhs2rids.finalize();
	rhs2rids.finalize();
	class2rids.finalize();
	feat2rids.finalize();
}


void knowledge_base_t::write_spec(const filepath_t &path) const
{
	std::ofstream fo(path);

	fo << "version: " << m_version << std::endl;
	fo << "time-stamp: " << INIT_TIME.string() << std::endl;
	fo << "num: " << rules.size() << std::endl;
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

} // end dav

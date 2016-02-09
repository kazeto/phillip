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


functional_predicate_configuration_t::functional_predicate_configuration_t()
: m_pid(INVALID_PREDICATE_ID), m_rel(REL_NONE)
{}


functional_predicate_configuration_t::functional_predicate_configuration_t(predicate_id_t pid, relation_flags_t rel)
    : m_pid(pid), m_rel(rel)
{
    auto p = kb()->predicates.id2pred(pid).to_arity();
    assign_unifiability(rel, p.second);
}


functional_predicate_configuration_t::functional_predicate_configuration_t(const sexp::sexp_t &s)
: m_pid(INVALID_PREDICATE_ID), m_rel(REL_NONE)
{
    // EXAMPLE: (define-functional-predicate (nsubj/3 asymmetric right-unique))

    if (s.children().size() == 2)
    {
        const sexp::sexp_t &def = s.child(1);
        predicate_with_arity_t a = def.child(0).string();
        predicate_t p;
        arity_t n;

        if (a.to_arity(&p, &n))
        {
            m_pid = kb::kb()->predicates.add(a);

            for (auto it = ++def.children().begin(); it != def.children().end(); ++it)
            {
                string_t rs = (*it)->string().to_lower();

                if (rs == "irreflexive")
                    m_rel |= REL_IRREFLEXIVE;
                else if (rs == "symmetric")
                    m_rel |= REL_SYMMETRIC;
                else if (rs == "asymmetric")
                    m_rel |= REL_ASYMMETRIC;
                else if (rs == "transitive")
                    m_rel |= REL_TRANSITIVE;
                else if (rs == "right-unique")
                    m_rel |= REL_RIGHT_UNIQUE;
                else
                    util::print_warning_fmt(
                    "phil::kb::functional_predicate_configuration_t: "
                    "The relation property identifier \"%s\" is invalid and skipped.", rs);
            }

            assign_unifiability(m_rel, n);
        }
    }
}


functional_predicate_configuration_t::functional_predicate_configuration_t(std::ifstream *fi)
{
    arity_t n;

    fi->read((char*)&m_pid, sizeof(predicate_id_t));
    fi->read((char*)&n, sizeof(arity_t));
    fi->read((char*)&m_rel, sizeof(relation_flags_t));

    assign_unifiability(m_rel, n);
}


void functional_predicate_configuration_t::write(std::ofstream *fo) const
{
    arity_t n = static_cast<arity_t>(m_unifiability.size());

    fo->write((char*)&m_pid, sizeof(predicate_id_t));
    fo->write((char*)&n, sizeof(arity_t));
    fo->write((char*)&m_rel, sizeof(relation_flags_t));
}


bool functional_predicate_configuration_t::do_postpone(
    const pg::proof_graph_t *graph, index_t n1, index_t n2) const
{
#ifdef DISABLE_UNIPP
    return false;
#else
    const literal_t &l1 = graph->node(n1).literal();
    const literal_t &l2 = graph->node(n2).literal();
    int n_all(0), n_fail(0);

    assert(graph->node(n1).arity_id() == m_pid);
    assert(graph->node(n2).arity_id() == m_pid);

    for (int i = 0; i < m_unifiability.size(); ++i)
    {
        variable_unifiability_type_e u = m_unifiability.at(i);

        if (u = UNI_UNLIMITED) continue;

        bool is_unified = (graph->find_sub_node(l1.terms.at(i), l2.terms.at(i)) < 0);

        switch (u)
        {
        case UNI_STRONGLY_LIMITED:
            if (not is_unified)
                return true;
            break;
        case UNI_WEAKLY_LIMITED:
            ++n_all;
            if (not is_unified)
                ++n_fail;
            break;
        }
    }

    return (n_all > 0 and n_fail == n_all);
#endif
}


string_t functional_predicate_configuration_t::repr() const
{
    // EXAMPLE: (define-functional-predicate (nsubj/3 asymmetric right-unique))
    string_t out =
        "(define-functional-predicate (" +
        kb()->predicates.id2pred(m_pid);

    if (m_rel & REL_IRREFLEXIVE)  out += " irreflexive";
    if (m_rel & REL_SYMMETRIC)    out += " symmetric";
    if (m_rel & REL_ASYMMETRIC)   out += " asymmetric";
    if (m_rel & REL_TRANSITIVE)   out += " transitive";
    if (m_rel & REL_RIGHT_UNIQUE) out += " right-unique";

    return out + "))";
}


void functional_predicate_configuration_t::
assign_unifiability(relation_flags_t flags, arity_t n)
{
    if (n == 2 or n == 3)
    {
        if (flags & REL_RIGHT_UNIQUE)
            m_unifiability.assign({ UNI_UNLIMITED, UNI_STRONGLY_LIMITED, UNI_UNLIMITED });
        else
            m_unifiability.assign({ UNI_UNLIMITED, UNI_WEAKLY_LIMITED, UNI_WEAKLY_LIMITED });
    }
    else
        m_pid = INVALID_PREDICATE_ID;
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
      m_cdb_arity_patterns(filename + ".pattern.cdb"),
      m_cdb_pattern_to_ids(filename + ".search.cdb"),
      axioms(filename),
      predicates(filename + ".arity.dat"),
      m_rm(filename + ".rm.dat")
{
    m_distance_provider = { NULL, "" };

    m_config_for_compile.max_distance = ph->param_float("kb_max_distance", -1.0);
    m_config_for_compile.thread_num = ph->param_int("kb_thread_num", 1);
    m_config_for_compile.do_disable_stop_word = ph->flag("disable_stop_word");
    m_config_for_compile.can_deduction = ph->flag("enable_deduction");
    m_config_for_compile.do_print_reachability_matrix = ph->flag("print-reachability-matrix");

    if (m_config_for_compile.thread_num < 1)
        m_config_for_compile.thread_num = 1;

    std::string dist_key = ph->param("distance_provider");
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
        m_cdb_rhs.prepare_compile();
        m_cdb_lhs.prepare_compile();
        m_cdb_arity_patterns.prepare_compile();
        m_cdb_pattern_to_ids.prepare_compile();

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
        read_axiom_group();
        predicates.read();

        axioms.prepare_query();
        m_cdb_rhs.prepare_query();
        m_cdb_lhs.prepare_query();
        m_cdb_arity_patterns.prepare_query();
        m_cdb_pattern_to_ids.prepare_query();
        m_rm.prepare_query();

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

        extend_inconsistency();

        insert_cdb(m_rhs_to_axioms, &m_cdb_rhs);
        insert_cdb(m_lhs_to_axioms, &m_cdb_lhs);
        write_axiom_group();

        m_rhs_to_axioms.clear();
        m_lhs_to_axioms.clear();
        m_group_to_axioms.clear();

        build_conjunct_predicates_map();
        create_reachable_matrix();
        write_config();
        predicates.write();

        if (m_config_for_compile.do_print_reachability_matrix)
        {
            std::cerr << "Reachability Matrix:" << std::endl;
            m_rm.prepare_query();

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
                    float dist = m_rm.get(idx1, idx2);
                    std::cerr << std::setw(a2.length()) << dist << " | ";
                }
                std::cerr << std::endl;
            }
        }

        predicates.clear();
    }

    axioms.finalize();
    m_cdb_rhs.finalize();
    m_cdb_lhs.finalize();
    m_cdb_arity_patterns.finalize();
    m_cdb_pattern_to_ids.finalize();
    m_rm.finalize();
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
            predicates.add(br->literal().get_arity());

        axiom_id_t id = axioms.num_axioms();
        axioms.add(name, func);

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
            predicate_id_t arity_id = predicates.add((*it)->get_arity());
            m_rhs_to_axioms[arity_id].insert(id);
        }

        // DEDUCTION
        if (m_config_for_compile.can_deduction)
        {
            for (auto it = lhs.begin(); it != lhs.end(); ++it)
            if (not(*it)->is_equality())
            {
                predicate_id_t arity_id = predicates.add((*it)->get_arity());
                m_lhs_to_axioms[arity_id].insert(id);
            }
        }

        return id;
    }
    else
        return INVALID_AXIOM_ID;
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


void knowledge_base_t::search_arity_patterns(predicate_id_t arity, std::list<arity_pattern_t> *out) const
{
    if (not m_cdb_arity_patterns.is_readable())
    {
        util::print_warning("kb-search: Kb-state is invalid.");
        return;
    }

    size_t value_size;
    const char *value = (const char*)
        m_cdb_arity_patterns.get(&arity, sizeof(predicate_id_t), &value_size);

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


float knowledge_base_t::get_distance(
    const std::string &arity1, const std::string &arity2 ) const
{
    predicate_id_t get1 = predicates.pred2id(arity1);
    predicate_id_t get2 = predicates.pred2id(arity2);
    if (get1 == INVALID_PREDICATE_ID or get2 == INVALID_PREDICATE_ID) return -1.0f;

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


void knowledge_base_t::build_conjunct_predicates_map()
{
    IF_VERBOSE_1("Building the list of conjunct predicates...");

    axioms.prepare_query();
    m_cdb_rhs.prepare_query();
    m_cdb_lhs.prepare_query();

    std::map<predicate_id_t, std::set<arity_pattern_t> > arity_to_queries;
    std::map<arity_pattern_t, std::set< std::pair<axiom_id_t, bool> > > pattern_to_ids;

    auto proc = [this, &pattern_to_ids, &arity_to_queries](
        const lf::axiom_t &ax, bool is_backward)
    {
        std::vector<const lf::logical_function_t*> branches;
        hash_map<string_hash_t, std::set<std::pair<predicate_id_t, char> > > term2arity;
        arity_pattern_t query;

        ax.func.branch(is_backward ? 1 : 0).enumerate_literal_branches(&branches);

        for (index_t i = 0; i < branches.size(); ++i)
        {
            const literal_t &lit = branches[i]->literal();
            std::string arity = lit.get_arity();
            predicate_id_t idx = predicates.pred2id(arity);

            assert(idx != INVALID_PREDICATE_ID);
            std::get<0>(query).push_back(idx);

            for (int i_t = 0; i_t < lit.terms.size(); ++i_t)
            if (lit.terms.at(i_t).is_hard_term())
                term2arity[lit.terms.at(i_t)].insert(std::make_pair(i, (char)i_t));
        }

        // ENUMERATE HARD TERMS
        for (auto e : term2arity)
        {
            for (auto it1 = e.second.begin(); it1 != e.second.end(); ++it1)
            for (auto it2 = e.second.begin(); it2 != it1; ++it2)
                std::get<1>(query).push_back(util::make_sorted_pair(*it1, *it2));
        }
        std::get<1>(query).sort();

        pattern_to_ids[query].insert(std::make_pair(ax.id, is_backward));

        for (auto idx : std::get<0>(query))
        if (predicates.find_functional_predicate(idx) == nullptr)
            arity_to_queries[idx].insert(query);
    };

    for (axiom_id_t i = 0; i < axioms.num_axioms(); ++i)
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
            float progress = (float)(i)* 100.0f / (float)axioms.num_axioms();
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
            (char*)(&p.first), sizeof(predicate_id_t), value, size_value);

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
    IF_VERBOSE_1("Finished building the conjunct predicates.");
}


void knowledge_base_t::create_reachable_matrix()
{
    IF_VERBOSE_1("starts to create reachable matrix...");

    size_t processed(0), num_inserted(0);
    clock_t clock_past = clock_t();
    time_t time_start, time_end;
    time(&time_start);

    axioms.prepare_query();
    m_cdb_rhs.prepare_query();
    m_cdb_lhs.prepare_query();

    m_rm.prepare_compile();

    IF_VERBOSE_3(util::format("  num of axioms = %d", axioms.num_axioms()));
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

    for (axiom_id_t id = 0; id < axioms.num_axioms(); ++id)
    {
        if (++num_processed % 10 == 0 and phillip_main_t::verbose() >= VERBOSE_1)
        {
            float progress = (float)(num_processed)* 100.0f / (float)axioms.num_axioms();
            std::cerr << util::format("processed %d axioms [%.4f%%]\r", num_processed, progress);
        }

        lf::axiom_t axiom = get_axiom(id);
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
                    std::string arity = (*it_l)->get_arity();
                    predicate_id_t idx = predicates.pred2id(arity);

                    if (idx != INVALID_PREDICATE_ID)
                    if (ignored.count(idx) == 0)
                        lhs_ids.insert(idx);
                }

                for (auto it_r = rhs.begin(); it_r != rhs.end(); ++it_r)
                {
                    std::string arity = (*it_r)->get_arity();
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
    predicate_id_t arity_id, const util::cdb_data_t *dat) const
{
    std::list<axiom_id_t> out;

    if (arity_id != INVALID_PREDICATE_ID and dat != NULL)
    {
        if (not dat->is_readable())
            util::print_warning("kb-search: Kb-state is invalid.");
        else
        {
            size_t value_size;
            const char *value = (const char*)
                dat->get((char*)&arity_id, sizeof(predicate_id_t), &value_size);

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


void knowledge_base_t::axioms_database_t::add(
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



knowledge_base_t::predicate_database_t::predicate_database_t(const std::string &filename)
: m_filename(filename)
{
    m_arities.push_back("");
    m_arity2id[""] = INVALID_PREDICATE_ID;
}


void knowledge_base_t::predicate_database_t::clear()
{
    m_arities.assign(1, "");

    m_arity2id.clear();
    m_arity2id[""] = INVALID_PREDICATE_ID;

    m_functional_predicates.clear();
    m_mutual_exclusions.clear();
}


void knowledge_base_t::predicate_database_t::read()
{
    std::ifstream fi(m_filename.c_str(), std::ios::in | std::ios::binary);
    char line[256];

    clear();

    if (fi.bad())
        throw phillip_exception_t("Failed to open " + m_filename);

    {
        // READ PREDICATES LIST
        size_t arity_num;
        fi.read((char*)&arity_num, sizeof(size_t));

        for (size_t i = 0; i < arity_num; ++i)
        {
            unsigned char num_char;
            fi.read((char*)&num_char, sizeof(unsigned char));
            fi.read(line, sizeof(char)* num_char);
            line[num_char] = '\0';

            add(predicate_with_arity_t(line));
        }
    }

    {
        // READ FUNCTIONAL PREDICATES
        size_t unipp_num;
        fi.read((char*)&unipp_num, sizeof(size_t));

        for (size_t i = 0; i < unipp_num; ++i)
            define_functional_predicate(functional_predicate_configuration_t(&fi));
    }

    {
        // READ MUTUAL-EXCLUSIONS BETWEEN PREDICATES
        size_t muex_num_1;
        fi.read((char*)&muex_num_1, sizeof(size_t));

        for (size_t i = 0; i < muex_num_1; ++i)
        {
            size_t muex_num_2;
            predicate_id_t id1;
            fi.read((char*)&id1, sizeof(predicate_id_t));
            fi.read((char*)&muex_num_2, sizeof(size_t));

            for (size_t j = 0; j < muex_num_2; ++j)
            {
                predicate_id_t id2;
                small_size_t pairs_num;
                std::list<std::pair<term_idx_t, term_idx_t> > pairs;

                fi.read((char*)&id2, sizeof(predicate_id_t));
                fi.read((char*)&pairs_num, sizeof(small_size_t));

                for (small_size_t k = 0; k < pairs_num; ++k)
                {
                    term_idx_t terms[2];
                    fi.read((char*)terms, sizeof(term_idx_t)* 2);

                    pairs.push_back(std::make_pair(terms[0], terms[1]));
                }

                m_mutual_exclusions[id1][id2] = pairs;
            }
        }
    }
}


void knowledge_base_t::predicate_database_t::write() const
{
    std::ofstream fo(m_filename.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);

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

    size_t unipp_num = m_functional_predicates.size();
    fo.write((char*)&unipp_num, sizeof(size_t));

    for (auto p : m_functional_predicates)
        p.second.write(&fo);

    size_t muex_num_1 = m_mutual_exclusions.size();
    fo.write((char*)&muex_num_1, sizeof(size_t));

    for (auto p1 : m_mutual_exclusions)
    {
        size_t muex_num_2 = p1.second.size();
        fo.write((char*)&p1.first, sizeof(predicate_id_t));
        fo.write((char*)&muex_num_2, sizeof(size_t));

        for (auto p2 : p1.second)
        {
            small_size_t pairs_num = p2.second.size();
            fo.write((char*)&p2.first, sizeof(predicate_id_t));
            fo.write((char*)&pairs_num, sizeof(small_size_t));

            for (auto p3 : p2.second)
            {
                term_idx_t terms[2] = { p3.first, p3.second };
                fo.write((char*)terms, sizeof(term_idx_t)* 2);
            }
        }
    }
}


predicate_id_t knowledge_base_t::predicate_database_t::add(const predicate_with_arity_t &arity)
{
    auto found = m_arity2id.find(arity);

    if (found != m_arity2id.end())
        return found->second;
    else
    {
        predicate_id_t id = m_arities.size();
        m_arity2id.insert(std::make_pair(arity, id));
        m_arities.push_back(arity);
        return id;
    }
}


void knowledge_base_t::predicate_database_t::define_functional_predicate(
    const functional_predicate_configuration_t &fp)
{
    if (fp.empty()) return;

    if (not kb()->is_writable())
    {
        util::print_warning_fmt(
            "SKIPPED: \"%s\"\n"
            "    -> The knowledge base is not writable.",
            id2pred(fp.arity_id()).c_str());
        return;
    }

    if (kb()->axioms.num_axioms() > 0)
    {
        util::print_warning_fmt(
            "SKIPPED: \"%s\"\n"
            "    -> Functional predicates must be defined before insertion of axioms.",
            id2pred(fp.arity_id()).c_str());
        return;
    }

    m_functional_predicates[fp.arity_id()] = fp;
}


void knowledge_base_t::predicate_database_t::define_mutual_exclusion(const lf::logical_function_t &f)
{
    if (not f.is_valid_as_inconsistency())
    {
        util::print_warning_fmt(
            "SKIPPED: \"%s\"\n"
            "    -> This logical formula is invalid as mutual exclusion.",
            f.to_string().c_str());
        return;
    }
    else
    {
        std::vector<const lf::logical_function_t*> lits;
        f.enumerate_literal_branches(&lits);
        define_mutual_exclusion(lits.at(0)->literal(), lits.at(1)->literal());
    }
}


void knowledge_base_t::predicate_database_t::define_mutual_exclusion(const literal_t &l1, const literal_t &l2)
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
        predicate_id_t a1 = add(l1.get_arity());
        predicate_id_t a2 = add(l2.get_arity());
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


void query_to_binary(const arity_pattern_t &q, std::vector<char> *bin)
{
    size_t size_expected =
        sizeof(unsigned char) * 3 +
        sizeof(predicate_id_t) * std::get<0>(q).size() +
        (sizeof(predicate_id_t) + sizeof(char)) * 2 * std::get<1>(q).size();
    size_t size = 0;
    bin->assign(size_expected, '\0');

    size += util::num_to_binary(std::get<0>(q).size(), &(*bin)[0]);
    for (auto id : std::get<0>(q))
        size += util::to_binary<predicate_id_t>(id, &(*bin)[0] + size);

    size += util::num_to_binary(std::get<1>(q).size(), &(*bin)[0] + size);
    for (auto ht : std::get<1>(q))
    {
        size += util::to_binary<predicate_id_t>(ht.first.first, &(*bin)[0] + size);
        size += util::to_binary<char>(ht.first.second, &(*bin)[0] + size);
        size += util::to_binary<predicate_id_t>(ht.second.first, &(*bin)[0] + size);
        size += util::to_binary<char>(ht.second.second, &(*bin)[0] + size);
    }

    assert(size == size_expected);
};


size_t binary_to_query(const char *bin, arity_pattern_t *out)
{
    size_t size(0);
    int num_arity, num_hardterm, num_option;

    std::get<0>(*out).clear();
    std::get<1>(*out).clear();

    size += util::binary_to_num(bin + size, &num_arity);
    for (int i = 0; i < num_arity; ++i)
    {
        predicate_id_t id;
        size += util::binary_to<predicate_id_t>(bin + size, &id);
        std::get<0>(*out).push_back(id);
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
        std::get<1>(*out).push_back(std::make_pair(
            std::make_pair(id1, idx1), std::make_pair(id2, idx2)));
    }

    return size;
}


} // end kb

} // end phil

/* -*- coding: utf-8 -*- */

#include <cassert>
#include <cstring>
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "./kb.h"
#include "./phillip.h"


namespace phil
{

namespace kb
{


const int BUFFER_SIZE = 512 * 512;


knowledge_base_t::knowledge_base_t(
    const std::string &filename, distance_provider_type_e dist,
    float max_distance)
    : m_state(STATE_NULL),
      m_filename(filename), m_max_distance(max_distance),
      m_cdb_id(filename + ".id.cdb"),
      m_cdb_name(filename +".name.cdb"),
      m_cdb_rhs(filename + ".rhs.cdb"),
      m_cdb_lhs(filename + ".lhs.cdb"),
      m_cdb_inc_pred(filename + ".inc.pred.cdb"),
      m_cdb_axiom_group(filename + ".group.cdb"),
      m_cdb_rm_idx(filename + ".rm.cdb"),
      m_rm(filename + ".rm.dat"),
      m_rm_dist(new basic_distance_provider_t()),
      m_num_compiled_axioms(0), m_num_temporary_axioms(0),
      m_num_unnamed_axioms(0)
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
        m_cdb_id.prepare_compile();
        m_cdb_name.prepare_compile();
        m_cdb_rhs.prepare_compile();
        m_cdb_lhs.prepare_compile();
        m_cdb_inc_pred.prepare_compile();
        m_cdb_axiom_group.prepare_compile();
        m_cdb_rm_idx.prepare_compile();

        m_num_compiled_axioms = 0;
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

        m_cdb_id.prepare_query();
        m_cdb_name.prepare_query();
        m_cdb_rhs.prepare_query();
        m_cdb_lhs.prepare_query();
        m_cdb_inc_pred.prepare_query();
        m_cdb_axiom_group.prepare_query();
        m_cdb_rm_idx.prepare_query();
        m_rm.prepare_query();

        m_num_compiled_axioms = m_cdb_id.size();
        m_state = STATE_QUERY;
    }
}


void knowledge_base_t::finalize()
{
    if (m_state == NULL) return;

    if (m_state == STATE_COMPILE)
    {
        _insert_cdb(m_name_to_axioms, &m_cdb_name);
        _insert_cdb(m_rhs_to_axioms, &m_cdb_rhs);
        _insert_cdb(m_lhs_to_axioms, &m_cdb_lhs);
        _insert_cdb(m_inc_to_axioms, &m_cdb_inc_pred);
        insert_axiom_group_to_cdb();

        m_name_to_axioms.clear();
        m_rhs_to_axioms.clear();
        m_lhs_to_axioms.clear();
        m_inc_to_axioms.clear();
        m_group_to_axioms.clear();

        create_reachable_matrix();
        write_config((m_filename + ".conf").c_str());

        m_arity_set.clear();
    }

    m_cdb_id.finalize();
    m_cdb_name.finalize();
    m_cdb_rhs.finalize();
    m_cdb_lhs.finalize();
    m_cdb_inc_pred.finalize();
    m_cdb_axiom_group.finalize();
    m_cdb_rm_idx.finalize();
    m_rm.finalize();

    m_state = STATE_NULL;
}


void knowledge_base_t::write_config(const char *filename) const
{
    std::ofstream fo(
        filename, std::ios::out | std::ios::trunc | std::ios::binary);
    char dist_type(m_rm_dist->type());

    fo.write((char*)&m_max_distance, sizeof(float));
    fo.write(&dist_type, sizeof(char));
    fo.close();
}


void knowledge_base_t::read_config(const char *filename)
{
    std::ifstream fi(filename, std::ios::in | std::ios::binary);
    char dist_type;

    fi.read((char*)&m_max_distance, sizeof(float));
    fi.read(&dist_type, sizeof(char));
    fi.close();

    set_distance_provider(static_cast<distance_provider_type_e>(dist_type));
}


void knowledge_base_t::insert_implication_for_compile(
    const lf::logical_function_t &lf, std::string name )
{
    if (not _can_insert_axiom_to_compile()) return;

    axiom_id_t id = m_num_compiled_axioms;
    if (name.empty()) name = _get_name_of_unnamed_axiom();

    _insert_cdb(name, lf);
    m_name_to_axioms[name].insert(id);

    // REGISTER AXIOMS'S GROUPS
    auto spl = split(name, "#");
    if (spl.size() > 1)
    {
        for (int i = 0; i < spl.size() - 1; ++i)
            m_group_to_axioms[spl[i]].insert(id);
    }
    
    std::vector<const literal_t*> rhs(lf.get_rhs()), lhs(lf.get_lhs());

    for( auto it=rhs.begin(); it!=rhs.end(); ++it )
    {
        std::string arity((*it)->get_predicate_arity());
        m_rhs_to_axioms[arity].insert(id);
        insert_arity(arity);
    }

    for( auto it=lhs.begin(); it!=lhs.end(); ++it )
    {
        std::string arity = (*it)->get_predicate_arity();
        m_lhs_to_axioms[arity].insert(id);
        insert_arity(arity);
    }
}


void knowledge_base_t::insert_inconsistency_for_compile(
    const lf::logical_function_t &lf, std::string name )
{
    if (not _can_insert_axiom_to_compile()) return;

    axiom_id_t id = m_num_compiled_axioms;
    if (name.empty()) name = _get_name_of_unnamed_axiom();

    _insert_cdb(name, lf);
    
    /* REGISTER AXIOM-ID TO MAP FOR INC */
    std::vector<const literal_t*> literals = lf.get_all_literals();
    for( auto it=literals.begin(); it!=literals.end(); ++it )
    {
        std::string arity = (*it)->get_predicate_arity();
        m_inc_to_axioms[arity].insert(id);
    }
}


void knowledge_base_t::insert_implication_temporary(
    const lf::logical_function_t &lf, std::string name)
{
    axiom_id_t id = m_num_temporary_axioms;
    if (name.empty()) name = _get_name_of_unnamed_axiom();

    _insert_axiom_temporary(lf, name);

    std::vector<const literal_t*>
        rhs(lf.branch(1).get_all_literals()),
        lhs(lf.branch(0).get_all_literals());

    for (auto it = rhs.begin(); it != rhs.end(); ++it)
    {
        std::string arity((*it)->get_predicate_arity());
        m_rhs_to_tmp_axioms[arity].insert(id);
    }

    for (auto it = lhs.begin(); it != lhs.end(); ++it)
    {
        std::string arity = (*it)->get_predicate_arity();
        m_lhs_to_tmp_axioms[arity].insert(id);
    }
}


void knowledge_base_t::insert_inconsistency_temporary(
    const lf::logical_function_t &lf, std::string name)
{
    axiom_id_t id = m_num_temporary_axioms;
    if (name.empty()) name = _get_name_of_unnamed_axiom();

    _insert_axiom_temporary(lf, name);

    /* REGISTER AXIOM-ID TO MAP FOR INC */
    std::vector<const literal_t*> literals = lf.get_all_literals();
    for (auto it = literals.begin(); it != literals.end(); ++it)
    {
        std::string arity = (*it)->get_predicate_arity();
        m_inc_to_tmp_axioms[arity].insert(id);
    }
}


lf::axiom_t knowledge_base_t::get_axiom(axiom_id_t id) const
{
    lf::axiom_t out;

    if (not m_cdb_id.is_readable())
    {
        print_warning("kb-search: KB is currently not readable.");
        return out;
    }

    if (id < m_num_compiled_axioms)
    {

        size_t value_size;
        const char *value = (const char*)
            m_cdb_id.get(&id, sizeof(axiom_id_t), &value_size);

        if (value == NULL)
        {
            std::string message = format(
                "kb-search: Axiom-ID \"%ld\" is not found!", id);
            print_warning(message);
            return out;
        }

        size_t size = out.func.read_binary(value);
        size += binary_to<axiom_id_t>(value + size, &out.id);
        size += binary_to_string(value + size, &out.name);
    }
    else
        out = m_temporary_axioms.at(id);
    
    return out;
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

        auto ids = search_id_list(grp, &m_cdb_axiom_group, NULL);
        out.insert(ids.begin(), ids.end());
    }

    return out;
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

    return m_rm.get(*get1, *get2);
}


void knowledge_base_t::_insert_cdb(
    const std::string &name, const lf::logical_function_t &lf)
{
    const int SIZE(512 * 512);
    char buffer[SIZE];

    /* AXIOM => BINARY-DATA */
    size_t size = lf.write_binary( buffer );
    size += to_binary<axiom_id_t>(
        m_num_compiled_axioms, buffer + size);
    size += string_to_binary(
        (name.empty() ? _get_name_of_unnamed_axiom() : name),
        buffer + size);
    assert( size < BUFFER_SIZE );

    /* INSERT AXIOM TO CDB.ID */
    m_cdb_id.put(&m_num_compiled_axioms, sizeof(axiom_id_t), buffer, size);
    ++m_num_compiled_axioms;
}


void knowledge_base_t::_insert_cdb(
    const hash_map<std::string, hash_set<axiom_id_t> > &ids,
    cdb_data_t *dat)
{
    const int SIZE( 512 * 512 );
    char buffer[SIZE];

   std::cerr
        << time_stamp() << "starts writing " << dat->filename() << "..."
        << std::endl;

    for( auto it=ids.begin(); it!=ids.end(); ++it )
    {
        size_t read_size = sizeof(size_t) + sizeof(axiom_id_t) * it->second.size();
        assert( read_size < SIZE );

        int size = to_binary<size_t>( it->second.size(), buffer );
        for( auto id=it->second.begin(); id!=it->second.end(); ++id )
            size += to_binary<axiom_id_t>(*id, buffer + size);

        dat->put(it->first.c_str(), it->first.length(), buffer, size);
    }

    std::cerr
        << time_stamp() << "completed writing "
        << dat->filename() << "." << std::endl;
}


void knowledge_base_t::_insert_axiom_temporary(
    const lf::logical_function_t &lf, std::string name)
{
    axiom_id_t id = get_axiom_num();
    lf::axiom_t *ax = &m_temporary_axioms[id];

    ax->id = id;
    ax->name = name;
    ax->func = lf;
    ++m_num_temporary_axioms;
};


bool knowledge_base_t::_can_insert_axiom_to_compile() const
{
    if (m_state != STATE_COMPILE or m_num_temporary_axioms > 0)
    {
        print_error("kb-insert: KB is currently not writable.");
        return false;
    }
    return true;
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

    std::cerr
        << time_stamp() << "starts writing " << dat.filename() << "..."
        << std::endl;

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

    std::cerr
        << time_stamp() << "completed writing "
        << dat.filename() << "." << std::endl;
}


void knowledge_base_t::create_reachable_matrix()
{
    print_console("starts to create reachable matrix...");

    size_t N(m_arity_set.size()), processed(0), num_inserted(0);
    clock_t clock_past = clock_t();
    time_t time_start, time_end;
    time(&time_start);

    print_console_fmt("  num of axioms = %d", m_num_compiled_axioms);
    print_console_fmt("  num of arities = %d", N);
    print_console_fmt("  max distance = %.2f", get_max_distance());

    m_cdb_id.prepare_query();
    m_cdb_rhs.prepare_query();
    m_cdb_lhs.prepare_query();
    m_cdb_inc_pred.prepare_query();
    m_cdb_rm_idx.prepare_query();

    m_rm.prepare_compile();

    print_console("  computing distance of direct edges...");
    hash_map<size_t, hash_map<size_t, float> > base;
    _create_reachable_matrix_direct(&base);

    print_console("  writing reachable matrix...");
    for (auto it = base.begin(); it != base.end(); ++it)
    {
        size_t idx = it->first;
        hash_map<size_t, float> dist;

        _create_reachable_matrix_indirect(idx, base, &dist);
        m_rm.put(idx, dist);

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
    }

    time(&time_end);
    int proc_time(time_end - time_start); 
    double coverage(num_inserted * 100.0 / (double)(N * N));
    
    print_console("completed computation.");
    print_console_fmt("  process-time = %d", proc_time);
    print_console_fmt("  coverage = %.6lf%%", coverage);
}


void knowledge_base_t::_create_reachable_matrix_direct(
    hash_map<size_t, hash_map<size_t, float> > *out)
{
    for (auto ar = m_arity_set.begin(); ar != m_arity_set.end(); ++ar)
    {
        size_t idx1 = *search_arity_index(*ar);
        hash_map<size_t, float> *target = &(*out)[idx1];
        std::list<axiom_id_t>
            ids_lhs(search_axioms_with_lhs(*ar)),
            ids_rhs(search_axioms_with_rhs(*ar));

        (*target)[idx1] = 0.0f;
    
        for (int i = 0; i < 2; ++i)
        {
            bool is_forward(i == 0);
            std::list<axiom_id_t> *ids = is_forward ? &ids_lhs : &ids_rhs;

            for (auto id = ids->begin(); id != ids->end(); ++id)
            {
                lf::axiom_t axiom = get_axiom(*id);
                std::vector<const literal_t*> lits =
                    axiom.func.branch(is_forward ? 1 : 0).get_all_literals();

                for (auto li = lits.begin(); li != lits.end(); ++li)
                {
                    std::string arity2 = (*li)->get_predicate_arity();
                    size_t idx2 = *search_arity_index(arity2);
                    float dist = (*m_rm_dist)(axiom);
                    
                    if (dist >= 0.0f)
                    {
                        bool do_add(false);
                        auto find = target->find(idx2);

                        if (find == target->end())    do_add = true;
                        else if (dist < find->second) do_add = true;

                        if (do_add)
                            (*target)[idx2] = dist;
                    }
                }
            }
        }
    }
}


void knowledge_base_t::_create_reachable_matrix_indirect(
    size_t idx1, hash_map<size_t, hash_map<size_t, float> > &base,
    hash_map<size_t, float> *out)
{
    hash_map<size_t, float> current;

    current[idx1] = 0.0f;
    (*out)[idx1] = 0.0f;

    while (not current.empty())
    {
        hash_map<size_t, float> next;

        for (auto it1 = current.begin(); it1 != current.end(); ++it1)
        {
            const hash_map<size_t, float> &dists = base.at(it1->first);

            for (auto it2 = dists.begin(); it2 != dists.end(); ++it2)
            {
                size_t idx2(it2->first);
                float dist(it1->second + it2->second); // DISTANCE idx1 ~ idx2
                
                if (get_max_distance() > 0.0f and dist > get_max_distance())
                    continue;
                
                auto find = out->find(idx2);
                if (find != out->end())
                if (dist >= find->second)
                    continue;

                if (dist < get_max_distance())
                    next[idx2] = dist;
                (*out)[idx2] = dist;
            }
        }

        current = next;
    }
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
    const std::string &query, const cdb_data_t *dat,
    const hash_map<std::string, hash_set<axiom_id_t> > *tmp) const
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

    if (tmp != NULL)
    {
        auto find = tmp->find(query);
        if (find != tmp->end())
            out.insert(out.end(), find->second.begin(), find->second.end());
    }
    
    return out;
}


knowledge_base_t::reachable_matrix_t::
    reachable_matrix_t(const std::string &filename)
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
        m_fout = new std::ofstream(
            m_filename.c_str(), std::ios::binary | std::ios::out);

        pos_t pos;
        m_fout->write((const char*)&pos, sizeof(pos_t));
    }
}


void knowledge_base_t::reachable_matrix_t::prepare_query()
{
    if (is_writable())
        finalize();

    if (not is_readable())
    {
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


float knowledge_base_t::
reachable_matrix_t::get(size_t idx1, size_t idx2) const
{
    if (idx1 > idx2) std::swap(idx1, idx2);

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

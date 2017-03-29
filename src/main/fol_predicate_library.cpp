#include "./fol.h"

namespace dav
{

std::unique_ptr<predicate_library_t> predicate_library_t::ms_instance;


predicate_library_t* predicate_library_t::instance()
{
    if (not ms_instance)
        ms_instance.reset(new predicate_library_t());
    return ms_instance.get();
}


predicate_library_t::predicate_library_t()
{
    init();
}


void predicate_library_t::init()
{
    m_predicates.clear();
    m_pred2id.clear();
    m_properties.clear();

    m_predicates.push_back(predicate_t());
    m_pred2id[""] = INVALID_PREDICATE_ID;

    m_predicates.push_back(predicate_t("=", 2));
    m_pred2id["=/2"] = EQ_PREDICATE_ID;
}


void predicate_library_t::load()
{
    std::ifstream fi(m_filename.c_str(), std::ios::in | std::ios::binary);
    char line[256];

    init();

    if (fi.bad())
        throw phillip_exception_t("Failed to open " + m_filename);

    {
        // READ PREDICATES LIST
        size_t num;
        fi.read((char*)&num, sizeof(size_t));

        for (size_t i = 0; i < num; ++i)
            add(predicate_t(&fi));
    }

    {
        // READ FUNCTIONAL PREDICATES
        size_t unipp_num;
        fi.read((char*)&unipp_num, sizeof(size_t));

        for (size_t i = 0; i < unipp_num; ++i)
            add_predicate_property(predicate_property_t(&fi));
    }
}


void predicate_library_t::write() const
{
    std::ofstream fo(m_filename.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);

    if (fo.bad())
        throw phillip_exception_t("Failed to open " + m_filename);

    size_t arity_num = m_predicates.size();
    fo.write((char*)&arity_num, sizeof(size_t));

    for (const auto &p : m_predicates)
        p.write(&fo);

    size_t unipp_num = m_properties.size();
    fo.write((char*)&unipp_num, sizeof(size_t));

    for (auto p : m_properties)
        p.second.write(&fo);
}


predicate_id_t predicate_library_t::add(const predicate_t &p)
{
    auto found = m_pred2id.find(p.string());

    if (found != m_pred2id.end())
        return found->second;
    else
    {
        predicate_id_t pid = m_predicates.size();

        m_pred2id.insert(std::make_pair(p.string(), pid));
        m_predicates.push_back(p);
        m_predicates.back().pid() = pid;

        return pid;
    }
}


predicate_id_t predicate_library_t::add(const literal_t &a)
{
    if (a.predicate().pid() == INVALID_PREDICATE_ID)
        return add(a.predicate());
    else
        return a.predicate().pid();
}


void predicate_library_t::add_predicate_property(const predicate_property_t &fp)
{
    if (not fp.good()) return;

    m_properties[fp.pid()] = fp;
}



} // end of dav
#include "./fol.h"

namespace dav
{

predicate_property_t::predicate_property_t()
    : m_pid(INVALID_PREDICATE_ID)
{}


predicate_property_t::predicate_property_t(predicate_id_t pid, const properties_t &prp)
    : m_pid(pid), m_properties(prp)
{
    auto p = predicate_library_t::instance()->id2pred(pid);
    assign_unifiability();
}


predicate_property_t::predicate_property_t(std::ifstream *fi)
{
    fi->read((char*)&m_pid, sizeof(predicate_id_t));

    small_size_t n;
    fi->read((char*)&n, sizeof(small_size_t));

    for (small_size_t i = 0; i < n; ++i)
    {
        char c;
        fi->read(&c, sizeof(char));
        m_properties.insert(static_cast<predicate_property_type_e>(c));
    }

    assign_unifiability();
}


void predicate_property_t::write(std::ofstream *fo) const
{
    fo->write((char*)&m_pid, sizeof(predicate_id_t));

    small_size_t n = static_cast<small_size_t>(m_properties.size());
    fo->write((char*)&n, sizeof(small_size_t));

    for (auto p : m_properties)
    {
        char c = static_cast<char>(p);
        fo->write(&c, sizeof(char));
    }
}


//bool predicate_property_t::do_postpone(
//    const pg::proof_graph_t *graph, index_t n1, index_t n2) const
//{
//#ifdef DISABLE_UNIPP
//    return false;
//#else
//    const atom_t &l1 = graph->node(n1).literal();
//    const atom_t &l2 = graph->node(n2).literal();
//    int n_all(0), n_fail(0);
//
//    assert(graph->node(n1).predicate_id() == m_pid);
//    assert(graph->node(n2).predicate_id() == m_pid);
//
//    for (int i = 0; i < m_unifiability.size(); ++i)
//    {
//        unifiability_type_e u = m_unifiability.at(i);
//
//        if (u = UNI_UNLIMITED) continue;
//
//        bool is_unified =
//            (graph->find_sub_node(l1.terms().at(i), l2.terms().at(i)) < 0);
//
//        switch (u)
//        {
//        case UNI_STRONGLY_LIMITED:
//            if (not is_unified)
//                return true;
//            break;
//        case UNI_WEAKLY_LIMITED:
//            ++n_all;
//            if (not is_unified)
//                ++n_fail;
//            break;
//        }
//    }
//
//    return (n_all > 0 and n_fail == n_all);
//#endif
//}


bool predicate_property_t::good() const
{
    return
        (m_pid != INVALID_PREDICATE_ID) and
        not (is_symmetric() and is_asymmetric()) and
        not (is_symmetric() and is_right_unique());
}



string_t predicate_property_t::string() const
{
    // EXAMPLE: (define-functional-predicate (nsubj/3 asymmetric right-unique))

    std::list<std::string> strs;
    for (auto p : m_properties)
    {
        switch (p)
        {
        case PRP_IRREFLEXIVE: strs.push_back("irreflexive"); break;
        case PRP_SYMMETRIC: strs.push_back("symmetric"); break;
        case PRP_ASYMMETRIC: strs.push_back("asymmetric"); break;
        case PRP_TRANSITIVE: strs.push_back("transitive"); break;
        case PRP_RIGHT_UNIQUE: strs.push_back("right_unique"); break;
        }
    }
    
	const auto &pred = predicate_library_t::instance()->id2pred(m_pid);

    return
        "property " + pred.string() +
        " : {" + join(strs.begin(), strs.end(), ", ") + "}";
}


void predicate_property_t::assign_unifiability()
{
    const predicate_t &p = predicate_library_t::instance()->id2pred(m_pid);

    if (p.arity() == 2)
    {
        // HERE, A LITERAL REPRESENTS A RELATION BETWEEN THE 1ST & 2ND TERMS.
        if (is_right_unique())
            m_unifiability.assign({ UNI_STRONGLY_LIMITED, UNI_UNLIMITED });
        else
            m_unifiability.assign({ UNI_WEAKLY_LIMITED, UNI_WEAKLY_LIMITED });
    }
    
    if (p.arity() == 3)
    {
        // HERE, A LITERAL REPRESENTS A RELATION BETWEEN THE 2ND & 3RD TERMS.
        // THE 1ST TERM IS A VARIABLE OF THE RELATION ITSELF.
        if (is_right_unique())
            m_unifiability.assign({ UNI_UNLIMITED, UNI_STRONGLY_LIMITED, UNI_UNLIMITED });
        else
            m_unifiability.assign({ UNI_UNLIMITED, UNI_WEAKLY_LIMITED, UNI_WEAKLY_LIMITED });
    }
}


}
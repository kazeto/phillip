/* -*- coding: utf-8 -*- */

#include <algorithm>
#include <cstring>
#include <cstdarg>
#include <sstream>

#include "logical_function.h"
#include "kb.h"


namespace phil
{

namespace lf
{


const std::string OPR_STR_NAME = "name";
const std::string OPR_STR_AND = "^";
const std::string OPR_STR_OR = "v";
const std::string OPR_STR_IMPLICATION = "=>";
const std::string OPR_STR_INCONSISTENT = "xor";
const std::string OPR_STR_REQUIREMENT = "req";
const std::string OPR_STR_UNIPP = "unipp";
const std::string OPR_STR_EXARGSET = "argset";
const std::string OPR_STR_ASSERTION = "assert";


bool logical_function_t::check_validity_of_conjunction(
    const std::list<const logical_function_t*> &conj, bool do_allow_no_content_literals)
{
    hash_set<term_t> terms_c; // TEMRS IN CONTENT-LITERALS
    std::list<std::pair<const term_t*, const term_t*>> terms_f; // TERMS IN FUNCTIONAL-LITERALS

    for (const auto &f : conj)
    {
        if (f->is_operator(OPR_LITERAL))
        if (f->literal().is_valid())
        {
            const literal_t &lit = f->literal();
            auto fp = kb::kb()->predicates.find_functional_predicate(lit.pid());

            if (fp == nullptr)
                terms_c.insert(lit.terms().begin(), lit.terms().end());
            else if (fp->is_right_unique())
                terms_f.push_back(std::make_pair(
                &lit.terms().at(fp->governor()), nullptr));
            else
                terms_f.push_back(std::make_pair(
                &lit.terms().at(fp->governor()),
                &lit.terms().at(fp->dependent())));

            continue;
        }

        return false;
    }

    // IF THERE IS NO CONTENT-LITERAL, RETURNS TRUE.
    if (do_allow_no_content_literals)
    if (terms_c.empty())
        return true;

    // CHECK WHETHER THERE IS A PARENT OF EACH FUNCTIONAL-LITERAL
    for (const auto &p : terms_f)
    if (terms_c.count(*p.first) == 0)
    {
        if (p.second == nullptr)
            return false;
        else if (terms_c.count(*p.second) == 0)
            return false;
    }

    return true;
}


logical_function_t::logical_function_t(
    logical_operator_t opr, const std::vector<literal_t> &literals)
    : m_operator(opr)
{
    for (int i = 0; i < literals.size(); i++)
        m_branches.push_back(logical_function_t(literals.at(i)));
}


logical_function_t::logical_function_t(const sexp::sexp_t &s)
    : m_operator(OPR_UNSPECIFIED)
{
    if (s.is_functor(OPR_STR_IMPLICATION))
    {
        m_operator = OPR_IMPLICATION;
        m_branches.push_back(logical_function_t(s.child(1)));
        m_branches.push_back(logical_function_t(s.child(2)));
    }
    else if (s.is_functor(OPR_STR_INCONSISTENT))
    {
        m_operator = OPR_INCONSISTENT;
        m_branches.push_back(logical_function_t(s.child(1)));
        m_branches.push_back(logical_function_t(s.child(2)));
    }
    else if (s.is_functor(OPR_STR_AND) or s.is_functor(OPR_STR_OR))
    {
        m_operator = s.is_functor(OPR_STR_AND) ? OPR_AND : OPR_OR;
        for (auto it = ++s.children().cbegin(); it != s.children().cend(); ++it)
        {
            if (not(*it)->is_parameter())
                m_branches.push_back(logical_function_t(**it));
        }
    }
    else if (s.is_functor(OPR_STR_REQUIREMENT))
    {
        m_operator = OPR_REQUIREMENT;
        for (auto it = ++s.children().cbegin(); it != s.children().cend(); ++it)
        {
            if (not(*it)->is_parameter())
                m_branches.push_back(logical_function_t(**it));
        }
    }
    else if (s.is_functor(OPR_STR_UNIPP))
    {
        m_operator = OPR_UNIPP;
        for (auto it = ++s.children().cbegin(); it != s.children().cend(); ++it)
        {
            if (not(*it)->is_parameter())
                m_branches.push_back(logical_function_t(**it));
        }
    }
    else
    {
        // ASSUMING s IS LITERAL
        m_operator = OPR_LITERAL;
        m_literal = literal_t(s);
    }
    
    // SET OPTIONAL PARAMETER
    if (not s.children().empty())
    {
        const sexp::sexp_t &child = *(s.children().back());
        if (child.is_parameter())
            m_param = child.string();
    }
}


bool logical_function_t::param2int(int *out) const
{
    auto splitted = util::split(param(), ":");
    for (auto it = splitted.begin(); it != splitted.end(); ++it)
    {
        if (_sscanf(it->c_str(), "%d", out) == 1)
            return true;
    }
    return false;
}


bool logical_function_t::param2double(double *out) const
{
    auto splitted = util::split(param(), ":");
    for (auto it = splitted.begin(); it != splitted.end(); ++it)
    {
        if (_sscanf(it->c_str(), "%lf", out) == 1)
            return true;
    }
    return false;
}


std::string logical_function_t::repr() const
{
    std::function<void(const logical_function_t*, std::string*)> print;

    print = [&print](const logical_function_t *f, std::string *out)
    {
        switch (f->m_operator)
        {
        case OPR_LITERAL:
            (*out) += f->m_literal.to_string();
            break;
        case OPR_IMPLICATION:
            print(&f->m_branches[0], out);
            (*out) += " => ";
            print(&f->m_branches[1], out);
            break;
        case OPR_INCONSISTENT:
            print(&f->m_branches[0], out);
            (*out) += " xor ";
            print(&f->m_branches[1], out);
            break;
        case OPR_OR:
        case OPR_AND:
            for (auto it = f->m_branches.begin(); it != f->m_branches.end(); ++it)
            {
                if (it != f->m_branches.begin())
                    (*out) += f->is_operator(OPR_AND) ? " ^ " : " v ";

                bool is_literal = it->is_operator(OPR_LITERAL);

                if (not is_literal) (*out) += "(";
                print(&(*it), out);
                if (not is_literal) (*out) += ")";
            }
            break;
        case OPR_UNIPP:
            (*out) += "(uni-pp ";
            print(&f->m_branches[0], out);
            (*out) += ")";
            break;
        }
    };

    std::string out;
    print(this, &out);
    return out;
}


bool logical_function_t::do_include(const literal_t& lit) const
{
    auto my_literals(get_all_literals());
    for (unsigned i = 0; i < my_literals.size(); i++)
    {
        if (*my_literals[i] == lit)
            return true;
    }
    return false;
}


bool logical_function_t::find_parameter(const std::string &query) const
{
    if (m_param.empty()) return false;

    int idx(0);
    while ((idx = m_param.find(query, idx)) >= 0)
    {
        if (m_param.at(idx - 1) != ':')
            continue;
        if (idx + query.size() <= m_param.size())
            return true;
        else if (m_param.at(idx + query.size()) == ':')
            return true;
    }
    return false;
}


bool logical_function_t::scan_parameter(const std::string &format, ...) const
{
    if (m_param.empty()) return false;

    int idx1(1), idx2;
    while (idx1 < m_param.size())
    {
        idx2 = m_param.find(':', idx1);
        std::string sub =
            (idx2 != std::string::npos) ?
            m_param.substr(idx1, idx2 - idx1) : m_param.substr(idx1);

        va_list arg;
        va_start(arg, format);
        int ret = _vsscanf(sub.c_str(), format.c_str(), arg);
        va_end(arg);

        if (ret > 0) return true;

        if (idx2 != std::string::npos)
            idx1 = idx2 + 1;
        else
            break;
    }
    return false;
}


void logical_function_t::process_parameter(
    const std::function<bool(const std::string&)> &processor) const
{
    if (m_param.empty()) return;

    int idx1(1), idx2;
    while (idx1 < m_param.size())
    {
        idx2 = m_param.find(':', idx1);
        std::string sub =
            (idx2 != std::string::npos) ?
            m_param.substr(idx1, idx2 - idx1) : m_param.substr(idx1);

        if (processor(sub))
            return;
    }
}


bool logical_function_t::is_valid_as_observation() const
{
    if (not is_operator(OPR_AND)) return false;

    std::list<const logical_function_t*> conj;

    for (const auto &br : branches())
    {
        if (br.is_operator(OPR_LITERAL))
        if (br.literal().is_valid())
        {
            conj.push_back(&br);
            continue;
        }
        return false;
    }

    return check_validity_of_conjunction(conj, false);
}


bool logical_function_t::is_valid_as_implication() const
{
    if (not is_operator(OPR_IMPLICATION)) return false;
    if (branches().size() != 2) return false;

    std::list<const logical_function_t*> conj;

    // CHECK LHS & RHS
    for (const auto &br : branches())
    {
        if (br.is_operator(OPR_LITERAL))
        {
            if (br.literal().is_valid())
                conj.push_back(&br);
            else
                return false;
        }
        else if (br.is_operator(OPR_AND))
        {
            for (auto it = br.branches().cbegin(); it != br.branches().cend(); ++it)
            {
                if (it->is_operator(OPR_LITERAL))
                if (it->literal().is_valid())
                {
                    conj.push_back(&(*it));
                    continue;
                }

                return false;
            }
        }
        else
            return false;
    }

    return check_validity_of_conjunction(conj, true);
}


bool logical_function_t::is_valid_as_inconsistency() const
{
    if (branches().size() != 2) return false;

    for (auto it = branches().begin(); it != branches().end(); ++it)
    {
        if (not it->is_operator(OPR_LITERAL))
            return false;
        else if (it->literal().is_equality())
            return false;
    }

    return true;
}


bool logical_function_t::is_valid_as_requirements() const
{
    if (not is_operator(OPR_REQUIREMENT))
        return false;

    size_t num_gold(0);

    for (auto br : branches())
    {
        if (br.find_parameter("gold"))
            num_gold += 1;

        if (br.is_operator(OPR_LITERAL))
            continue;
        else if (br.is_operator(OPR_AND))
        {
            for (auto br2 : br.branches())
            if (not br2.is_operator(OPR_LITERAL))
                return false;
        }
        else
            return false;
    }

    if (branches().size() > 1 and num_gold > 1)
        return false;

    return true;
}


void logical_function_t::get_all_literals( std::list<literal_t> *out ) const
{
    auto literals = get_all_literals();
    for( auto li=literals.begin(); li!=literals.end(); ++li )
        out->push_back(**li);
}


void logical_function_t::get_all_literals_sub(
    std::vector<const literal_t*> *p_out_list) const
{
    switch (m_operator)
    {
    case OPR_LITERAL:
        p_out_list->push_back(&m_literal);
        break;
    case OPR_IMPLICATION:
    case OPR_INCONSISTENT:
        m_branches[0].get_all_literals_sub(p_out_list);
        m_branches[1].get_all_literals_sub(p_out_list);
        break;
    case OPR_OR:
    case OPR_AND:
    case OPR_REQUIREMENT:
    case OPR_UNIPP:
        for (int i = 0; i<m_branches.size(); i++)
            m_branches[i].get_all_literals_sub(p_out_list);
        break;
    default:
        break;
    }
}


void logical_function_t::enumerate_literal_branches(
    std::vector<const logical_function_t*> *out) const
{
    switch (m_operator)
    {
    case OPR_LITERAL:
        out->push_back(this);
        break;
    case OPR_IMPLICATION:
    case OPR_INCONSISTENT:
        m_branches[0].enumerate_literal_branches(out);
        m_branches[1].enumerate_literal_branches(out);
        break;
    case OPR_OR:
    case OPR_AND:
    case OPR_REQUIREMENT:
    case OPR_UNIPP:
        for (int i = 0; i<m_branches.size(); i++)
            m_branches[i].enumerate_literal_branches(out);
        break;
    default:
        break;
    }
}


size_t logical_function_t::write_binary( char *bin ) const
{
    size_t n(0);
    n += util::num_to_binary( static_cast<int>(m_operator), bin );

    switch( m_operator )
    {
    case OPR_LITERAL:
        n += m_literal.write_binary( bin+n );
        break;
    case OPR_AND:
    case OPR_OR:
        n += util::num_to_binary( m_branches.size(), bin+n );
        for( int i=0; i<m_branches.size(); ++i )
            n += m_branches.at(i).write_binary( bin+n );
        break;
    case OPR_IMPLICATION:
    case OPR_INCONSISTENT:
        n += m_branches.at(0).write_binary( bin+n );
        n += m_branches.at(1).write_binary( bin+n );
        break;
    case OPR_UNIPP:
        n += m_branches.at(0).write_binary( bin+n );
        break;
    default:
        break;
    }

    n += util::string_to_binary( m_param, bin+n );

    return n;
}


size_t logical_function_t::read_binary( const char *bin )
{
    size_t n(0);
    int i_buf;
    
    n += util::binary_to_num( bin, &i_buf );
    m_operator = static_cast<logical_operator_t>(i_buf);

    switch( m_operator )
    {
    case OPR_LITERAL:
        n += m_literal.read_binary( bin+n );
        break;
    case OPR_AND:
    case OPR_OR:
        n += util::binary_to_num(bin + n, &i_buf);
        m_branches.assign(i_buf, logical_function_t());
        for (int i = 0; i < i_buf; ++i)
            n += m_branches[i].read_binary(bin + n);
        break;
    case OPR_IMPLICATION:
    case OPR_INCONSISTENT:
        m_branches.assign(2, logical_function_t());
        n += m_branches[0].read_binary(bin + n);
        n += m_branches[1].read_binary(bin + n);
        break;
    case OPR_UNIPP:
        m_branches.assign(1, logical_function_t());
        n += m_branches[0].read_binary(bin + n);
    default:
        throw phillip_exception_t("Invalid operator occured.");
    }

    n += util::binary_to_string(bin + n, &m_param);

    return n;
}


void logical_function_t::add_branch(const logical_function_t &lf)
{
    m_branches.push_back(lf);
}



/* -------- Methods of parameter_splitter_t -------- */

parameter_splitter_t::parameter_splitter_t(const logical_function_t *master)
: m_master(master), m_idx1(0), m_idx2(0)
{
    const std::string &p = m_master->param();
    if (not is_end())
    {
        assert(p.at(0) == ':');
        m_idx2 = p.find(':', 1);
        m_substr = p.substr(1, m_idx2);
    }
}


parameter_splitter_t::parameter_splitter_t(const parameter_splitter_t &m)
: m_master(m.m_master), m_idx1(m.m_idx1), m_idx2(m.m_idx2)
{}


parameter_splitter_t& parameter_splitter_t::operator++()
{
    const std::string &p = m_master->param();
    index_t idx = p.find(':', m_idx2 + 1);

    m_idx1 = m_idx2;
    m_idx2 = idx;
    m_substr = p.substr(m_idx1 + 1, m_idx2 - m_idx1 - 1);

    return (*this);
}


parameter_splitter_t parameter_splitter_t::operator++(int)
{
    parameter_splitter_t ret = (*this);

    ++(*this);
    return ret;
}


void parse(const std::string &str, std::list<logical_function_t> *out)
{
    std::stringstream ss(str);
    sexp::reader_t reader(ss);

    for (; not reader.is_end(); reader.read())
    if (reader.is_root())
        out->push_back(logical_function_t(*reader.get_stack()));
}


}

}

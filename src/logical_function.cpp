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
const std::string OPR_STR_PARAPHRASE = "<=>";
const std::string OPR_STR_INCONSISTENT = "xor";
const std::string OPR_STR_REQUIREMENT = "req";
const std::string OPR_STR_UNIPP = "unipp";
const std::string OPR_STR_EXARGSET = "argset";


logical_function_t::logical_function_t(
    logical_operator_t opr, const std::vector<literal_t> &literals)
    : m_operator(opr)
{
    for (int i = 0; i < literals.size(); i++)
        m_branches.push_back(logical_function_t(literals.at(i)));
}


logical_function_t::logical_function_t(const sexp::stack_t &s)
    : m_operator(OPR_UNDERSPECIFIED)
{
    if (s.is_functor(OPR_STR_IMPLICATION))
    {
        m_operator = OPR_IMPLICATION;
        m_branches.push_back(logical_function_t(*(s.children[1])));
        m_branches.push_back(logical_function_t(*(s.children[2])));
    }
    else if (s.is_functor(OPR_STR_PARAPHRASE))
    {
        m_operator = OPR_PARAPHRASE;
        m_branches.push_back(logical_function_t(*(s.children[1])));
        m_branches.push_back(logical_function_t(*(s.children[2])));
    }
    else if (s.is_functor(OPR_STR_INCONSISTENT))
    {
        m_operator = OPR_INCONSISTENT;
        m_branches.push_back(logical_function_t(*(s.children[1])));
        m_branches.push_back(logical_function_t(*(s.children[2])));
    }
    else if (s.is_functor(OPR_STR_AND) or s.is_functor(OPR_STR_OR))
    {
        m_operator = s.is_functor(OPR_STR_AND) ? OPR_AND : OPR_OR;
        for (int i = 1; i < s.children.size(); i++)
        {
            const sexp::stack_t &child = *(s.children[i]);
            if (not child.is_parameter())
                m_branches.push_back(logical_function_t(child));
        }
    }
    else if (s.is_functor(OPR_STR_REQUIREMENT))
    {
        m_operator = OPR_REQUIREMENT;
        for (int i = 1; i < s.children.size(); i++)
        {
            const sexp::stack_t &child = *(s.children[i]);
            if (not child.is_parameter())
                m_branches.push_back(logical_function_t(child));
        }
    }
    else if (s.is_functor(OPR_STR_UNIPP))
    {
        m_operator = OPR_UNIPP;
        for (int i = 1; i < s.children.size(); i++)
        {
            const sexp::stack_t &child = *(s.children[i]);
            if (not child.is_parameter())
                m_branches.push_back(logical_function_t(child));
        }
    }
    else
    {
        // ASSUMING s IS LITERAL
        m_operator = OPR_LITERAL;
        m_literal = literal_t(s);
    }
    
    // SET OPTIONAL PARAMETER
    if( not s.children.empty() )
    {
        const sexp::stack_t &child = *(s.children.back());
        if (child.is_parameter())
            m_param = child.get_string();
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
    if (m_param.empty()) return "";

    int idx1(1), idx2;
    while (idx1 > 0)
    {
        va_list arg;
        idx2 = m_param.find(':');

        va_start(arg, format);
        int ret = _vsscanf(
            m_param.substr(idx1, idx2 - idx1).c_str(), format.c_str(), arg);
        va_end(arg);

        if (ret != EOF) return true;
        idx1 = idx2 + 1;
    }
    return false;
}


bool logical_function_t::is_valid_as_implication() const
{
    if (not is_operator(OPR_IMPLICATION))
        return false;
    if (branches().size() != 2)
        return false;

    // CHECKING LHS & RHS
    for (int i = 0; i < 2; ++i)
    {
        const logical_function_t &br = branch(i);

        if (br.is_operator(OPR_LITERAL))
            continue;
        else if (br.is_operator(OPR_AND))
        {
            for (auto it = br.branches().begin(); it != br.branches().end(); ++it)
            if (not it->is_operator(OPR_LITERAL))
                return false;
        }
        else
            return false;
    }

    return true;
}


bool logical_function_t::is_valid_as_paraphrase() const
{
    if (not is_operator(OPR_PARAPHRASE))
        return false;
    if (branches().size() != 2)
        return false;

    // CHECKING LHS & RHS
    for (int i = 0; i < 2; ++i)
    {
        const logical_function_t &br = branch(i);

        if (br.is_operator(OPR_LITERAL))
            continue;
        else if (br.is_operator(OPR_AND))
        {
            for (auto it = br.branches().begin(); it != br.branches().end(); ++it)
            if (not it->is_operator(OPR_LITERAL))
                return false;
        }
        else
            return false;
    }

    return true;
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


bool logical_function_t::is_valid_as_unification_postponement() const
{
    if (branches().size() != 1)
        return false;
    else
    {
        const logical_function_t &br = branch(0);
        if (not br.is_operator(OPR_LITERAL))
            return false;
        else
        {
            const literal_t &l = br.literal();
            term_t t1("."), t2("+"), t3("*");
            for (auto it = l.terms.begin(); it != l.terms.end(); ++it)
                if ((*it) != t1 and (*it) != t2 and (*it) != t3)
                    return false;
        }
    }

    return true;
}


bool logical_function_t::is_valid_as_argument_set() const
{
    if (not is_operator(OPR_LITERAL))
        return false;
    else
    {
        const std::vector<term_t> &terms(m_literal.terms);

        for (auto it_term = terms.begin(); it_term != terms.end(); ++it_term)
        {
            const std::string &str = it_term->string();
            int n_slash(0);

            for (auto c = str.rbegin(); c != str.rend() and n_slash < 2; ++c)
            {
                if (*c == '/')
                    ++n_slash;
                else if (not std::isdigit(*c))
                    return false;
            }
        }

        return true;
    }
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
            for (auto _br : br.branches())
            if (not br.is_operator(OPR_LITERAL))
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
    case OPR_PARAPHRASE:
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
    case OPR_PARAPHRASE:
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
    case OPR_PARAPHRASE:
    case OPR_INCONSISTENT:
        n += m_branches.at(0).write_binary( bin+n );
        n += m_branches.at(1).write_binary( bin+n );
        break;
    case OPR_UNIPP:
        n += m_branches.at(0).write_binary( bin+n );
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
    case OPR_PARAPHRASE:
    case OPR_INCONSISTENT:
        m_branches.assign(2, logical_function_t());
        n += m_branches[0].read_binary(bin + n);
        n += m_branches[1].read_binary(bin + n);
        break;
    case OPR_UNIPP:
        m_branches.assign(1, logical_function_t());
        n += m_branches[0].read_binary(bin + n);
    }

    n += util::binary_to_string(bin + n, &m_param);

    return n;
}


void logical_function_t::print(
    std::string *p_out_str, bool f_colored ) const
{
    switch( m_operator )
    {
    case OPR_LITERAL:
        (*p_out_str) += m_literal.to_string( f_colored );
        break;
    case OPR_IMPLICATION:
        m_branches[0].print(p_out_str, f_colored);
        (*p_out_str) += " => ";
        m_branches[1].print(p_out_str, f_colored);
        break;
    case OPR_PARAPHRASE:
        m_branches[0].print(p_out_str, f_colored);
        (*p_out_str) += " <=> ";
        m_branches[1].print(p_out_str, f_colored);
        break;
    case OPR_INCONSISTENT:
        m_branches[0].print( p_out_str, f_colored );
        (*p_out_str) += " xor ";
        m_branches[1].print( p_out_str, f_colored );
        break;
    case OPR_OR:
    case OPR_AND:
        for( auto it=m_branches.begin(); it!=m_branches.end(); ++it )
        {
            if( it != m_branches.begin() )
            {
                (*p_out_str) += (m_operator == OPR_AND) ? " ^ " : " v ";
                if( f_colored ) (*p_out_str) += "\n";
            }

            bool is_literal = it->is_operator(OPR_LITERAL);
            
            if( not is_literal ) (*p_out_str) += "(";
            it->print( p_out_str, f_colored );
            if( not is_literal ) (*p_out_str) += ")";
        }
        break;
    case OPR_UNIPP:
        (*p_out_str) += "(uni-pp ";
        m_branches[0].print(p_out_str, f_colored);
        (*p_out_str) += ")";
        break;
    }
}


void logical_function_t::add_branch(const logical_function_t &lf)
{
    m_branches.push_back(lf);
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

#pragma once

namespace phil
{

namespace lf
{


inline logical_function_t::logical_function_t()
: m_operator(OPR_UNDERSPECIFIED)
{}


inline logical_function_t::logical_function_t(logical_operator_t opr)
: m_operator(opr)
{}


inline logical_function_t::logical_function_t(const literal_t &lit)
: m_operator(OPR_LITERAL), m_literal(lit)
{}


inline bool logical_function_t::is_operator( logical_operator_t opr ) const
{
    return m_operator == opr;
}


inline const std::vector<logical_function_t>&
logical_function_t::branches() const
{
    return m_branches;
}


inline const logical_function_t& logical_function_t::branch(int i) const
{
    return m_branches.at(i);
}


inline const literal_t& logical_function_t::literal() const
{
    return m_literal;
}


inline const std::string& logical_function_t::param() const
{
    return m_param;
}


inline std::vector<const literal_t*> logical_function_t::get_lhs() const
{
    return m_branches.at(0).get_all_literals();
}


inline std::vector<const literal_t*> logical_function_t::get_rhs() const
{
    return m_branches.at(1).get_all_literals();
}


inline std::string logical_function_t::to_string( bool f_colored ) const
{
    std::string out;
    print(&out, f_colored);
    return out;
}


inline std::vector<const literal_t*>
logical_function_t::get_all_literals() const
{
    std::vector<const literal_t*> out;
    get_all_literals_sub(&out);
    return out;
}


}

}


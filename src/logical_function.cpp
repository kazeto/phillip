/* -*- coding: utf-8 -*- */


#include <cstring>
#include "logical_function.h"


namespace phil
{

namespace lf
{


logical_function_t::logical_function_t(
    logical_operator_t opr, const std::vector<literal_t> &literals )
    : m_operator( opr )
{
    for( int i=0; i<literals.size(); i++ )
        m_branches.push_back( logical_function_t( literals.at(i) ) );
}


logical_function_t::logical_function_t( const sexp::stack_t &s )
    : m_operator( OPR_UNDERSPECIFIED )
{
    if( s.is_functor("=>") )
    {
        m_operator = OPR_IMPLICATION;
        m_branches.push_back( logical_function_t( *(s.children[1]) ) );
        m_branches.push_back( logical_function_t( *(s.children[2]) ) );
    }
    else if( s.is_functor("_|_") )
    {
        m_operator = OPR_INCONSISTENT;
        m_branches.push_back( logical_function_t( *(s.children[1]) ) );
        m_branches.push_back( logical_function_t( *(s.children[2]) ) );
    }
    else if( s.is_functor("^") or s.is_functor("v") )
    {
        m_operator = s.is_functor("^") ? OPR_AND : OPR_OR;
        for( int i=1; i<s.children.size(); i++ )
        {
            const sexp::stack_t &child = *(s.children[i]);
            if( not child.is_parameter() )
                m_branches.push_back( logical_function_t(child) );
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
        if( child.is_parameter() )
            m_param = child.get_string();
    }
}


bool logical_function_t::do_include( const literal_t& lit ) const
{
    auto my_literals( get_all_literals() );
    for( unsigned i=0; i<my_literals.size(); i++ )
    {
        if( *my_literals[i] == lit )
            return true;
    }
    return false;
}


void logical_function_t::get_all_literals( std::list<literal_t> *out ) const
{
    auto literals = get_all_literals();
    for( auto li=literals.begin(); li!=literals.end(); ++li )
        out->push_back(**li);
}


void logical_function_t::get_all_literals_sub(
    std::vector<const literal_t*> *p_out_list ) const
{
    switch( m_operator )
    {
    case OPR_LITERAL:
        p_out_list->push_back( &m_literal );
        break;
    case OPR_IMPLICATION:
    case OPR_INCONSISTENT:
        m_branches[0].get_all_literals_sub( p_out_list );
        m_branches[1].get_all_literals_sub( p_out_list );
        break;
    case OPR_OR:
    case OPR_AND:
        for( int i=0; i<m_branches.size(); i++ )
            m_branches[i].get_all_literals_sub( p_out_list );
        break;
    }
}


size_t logical_function_t::write_binary( char *bin ) const
{
    size_t n(0);
    n += num_to_binary( static_cast<int>(m_operator), bin );

    switch( m_operator )
    {
    case OPR_LITERAL:
        n += m_literal.write_binary( bin+n );
        break;
    case OPR_AND:
    case OPR_OR:
        n += num_to_binary( m_branches.size(), bin+n );
        for( int i=0; i<m_branches.size(); ++i )
            n += m_branches.at(i).write_binary( bin+n );
        break;
    case OPR_IMPLICATION:
    case OPR_INCONSISTENT:
        n += m_branches.at(0).write_binary( bin+n );
        n += m_branches.at(1).write_binary( bin+n );
        break;
    }

    n += string_to_binary( m_param, bin+n );

    return n;
}


size_t logical_function_t::read_binary( const char *bin )
{
    size_t n(0);
    int i_buf;
    
    n += binary_to_num( bin, &i_buf );
    m_operator = static_cast<logical_operator_t>(i_buf);

    switch( m_operator )
    {
    case OPR_LITERAL:
        n += m_literal.read_binary( bin+n );
        break;
    case OPR_AND:
    case OPR_OR:
        n += binary_to_num( bin+n, &i_buf );
        m_branches.assign( i_buf, logical_function_t() );
        for( int i=0; i<i_buf; ++i )
            n += m_branches[i].read_binary( bin+n );
        break;
    case OPR_IMPLICATION:
    case OPR_INCONSISTENT:
        m_branches.assign( 2, logical_function_t() );
        n += m_branches[0].read_binary( bin+n );
        n += m_branches[1].read_binary( bin+n );
        break;
    }

    n += binary_to_string( bin+n, &m_param );

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
        m_branches[0].print( p_out_str, f_colored );
        (*p_out_str) += " => ";
        m_branches[1].print( p_out_str, f_colored );
        break;
    case OPR_INCONSISTENT:
        m_branches[0].print( p_out_str, f_colored );
        (*p_out_str) += " _|_ ";
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
    }
}


}

}

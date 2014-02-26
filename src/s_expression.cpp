/* -*- coding: utf-8 -*- */


#include <iostream>
#include "./s_expression.h"

namespace phil
{

namespace sexp
{


int stack_t::find_functor( const std::string &func_name ) const
{
    for( int i=0; i<children.size(); ++i )
        if( children[i]->is_functor( func_name ) )
            return i;
    return -1;
}


void stack_t::print( std::string *p_out_str ) const
{
    switch( type )
    {
    case STRING_STACK:
        (*p_out_str) += str;
        break;
        
    case TUPLE_STACK:
        for( int i=0; i<children.size(); ++i )
            children[i]->print( p_out_str );
        break;
        
    case LIST_STACK:
        (*p_out_str) += "(";
        for( int i=0; i<children.size(); ++i )
        {
            children[i]->print( p_out_str );
            if( i < children.size()-1 )
                (*p_out_str) += " ";
        }
        (*p_out_str) += ")";
        break;
    }
}


/** Thanks for https://gist.github.com/240957. */
reader_t& reader_t::read()
{
    bool comment_flag = false;
    char last_c       = 0;
  
    while( m_stream.good() )
    {
        char c = m_stream.get();

        m_read_bytes = m_stream.tellg();
        if( '\n' == c ) m_line_num++;
    
        stack_t::stack_type_e type = m_stack.back()->type;
        if( type != stack_t::STRING_STACK and last_c != '\\' and c == ';' )
        {
            comment_flag = true;
            continue;
        }
        else if( comment_flag )
        {
            if( '\n' == c ) comment_flag = false;
            continue;
        }

        switch( type )
        {
        case stack_t::LIST_STACK:
        {
            if( c == '(' )
            {
                /* IF IT WERE TOP STACK, THEN CLEAR. */
                if( m_stack.size() == 1 ) clear_stack();
                m_stack.push_back( new_stack( stack_t(stack_t::LIST_STACK) ) );
            }
            else if( c == ')' )
            {
                if(m_stack.size() < 2)
                {
                    std::cerr << "Syntax error at " << m_line_num
                              << ": too many parentheses." << std::endl
                              << m_stack.back()->to_string() << std::endl;
                    throw;
                }
                m_stack[ m_stack.size()-2 ]
                    ->children.push_back( m_stack.back() );
                m_stack.pop_back();
                if( m_stack.back()->children[0]->type == stack_t::TUPLE_STACK and
                    m_stack.back()->children[0]->children[0]->str == "quote" )
                {
                    m_stack[ m_stack.size()-2 ]
                        ->children.push_back( m_stack.back() );
                    m_stack.pop_back();
                }
                m_stack_current = m_stack.back()->children.back();
                return *this;
            }
            else if( c == '"' )
                m_stack.push_back( new_stack( stack_t(stack_t::STRING_STACK) ) );
            else if( is_sexp_separator(c) )
                break;
            else
            {
                stack_t s(
                    stack_t::TUPLE_STACK, std::string(1, c), m_stack_list );
                m_stack.push_back( new_stack(s) );
            }
            break;
        }
        case stack_t::STRING_STACK:
        {
            if( c == '"' )
            {
                m_stack[ m_stack.size()-2 ]
                    ->children.push_back( m_stack.back() );
                m_stack.pop_back();
                if( m_stack.back()->children[0]->type == stack_t::TUPLE_STACK and
                    m_stack.back()->children[0]->children[0]->str == "quote" )
                {
                    m_stack[ m_stack.size()-2 ]
                        ->children.push_back( m_stack.back() );
                    m_stack.pop_back();
                }
            }
            else if( c == '\\' ) m_stack.back()->str += m_stream.get();
            else if( c != ';'  ) m_stack.back()->str += c;
            break;
        }
        case stack_t::TUPLE_STACK:
        {
            if( is_sexp_separator(c) )
            {
                stack_t *p_atom = m_stack.back();
                m_stack.pop_back();
                m_stack.back()->children.push_back(p_atom);
                if( m_stack.back()->children[0]->type == stack_t::TUPLE_STACK and
                    m_stack.back()->children[0]->children[0]->str == "quote" )
                {
                    m_stack[ m_stack.size()-2 ]
                        ->children.push_back( m_stack.back() );
                    m_stack.pop_back();
                }
                m_stream.unget();
            }
            else if( c == '\\' )
                m_stack.back()->children[0]->str += m_stream.get();
            else
                m_stack.back()->children[0]->str += c;
            break;
        }
        }
        last_c = c;
    }
    clear_stack();
    return *this;
}


}

}

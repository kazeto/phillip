/* -*- coding: utf-8 -*- */


#include <iostream>
#include <iterator>
#include <functional>

#include "./sexp.h"

namespace phil
{

namespace sexp
{


int sexp_t::find_functor( const std::string &func_name ) const
{
    for( int i=0; i<m_children.size(); ++i )
    if (m_children[i]->is_functor(func_name))
        return i;
    return -1;
}


string_t sexp_t::expr() const
{
    std::function<void(const sexp_t*, string_t*)> sub;

    sub = [&sub](const sexp_t *ptr, string_t *out)
    {
        switch (ptr->type())
        {
        case STRING_STACK:
            (*out) += ptr->m_str;
            break;

        case TUPLE_STACK:
            for (const auto &c : ptr->children())
                sub(c, out);
            break;

        case LIST_STACK:
            (*out) += "(";
            for (auto it = ptr->children().cbegin(); it != ptr->children().cend(); ++it)
            {
                sub(*it, out);
                if (std::next(it) == ptr->children().cend())
                    (*out) += " ";
            }
            (*out) += ")";
            break;
        }
    };

    string_t out;
    sub(this, &out);
    return out;
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
    
        sexp_t::stack_type_e type = m_stack.back()->type();
        if( type != sexp_t::STRING_STACK and last_c != '\\' and c == ';' )
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
        case sexp_t::LIST_STACK:
            if( c == '(' )
            {
                /* IF IT WERE TOP STACK, THEN CLEAR. */
                if( m_stack.size() == 1 ) clear_stack();
                m_stack.push_back(new_stack(sexp_t(sexp_t::LIST_STACK)));
            }
            else if (c == ')')
            {
                if (m_stack.size() < 2)
                {
                    std::cerr << "Syntax error at " << m_line_num
                        << ": too many parentheses." << std::endl
                        << m_stack.back()->expr() << std::endl;
                    throw;
                }
                m_stack[m_stack.size() - 2]->add(m_stack.back());
                m_stack.pop_back();
                if (m_stack.back()->child(0).type() == sexp_t::TUPLE_STACK and
                    m_stack.back()->child(0).child(0).m_str == "quote")
                {
                    m_stack[m_stack.size() - 2]->add(m_stack.back());
                    m_stack.pop_back();
                }
                m_stack_current = m_stack.back()->m_children.back();
                return *this;
            }
            else if (c == '"')
                m_stack.push_back(new_stack(sexp_t(sexp_t::STRING_STACK)));
            else if( is_sexp_separator(c) )
                break;
            else
                m_stack.push_back(new_stack(sexp_t(sexp_t::TUPLE_STACK, std::string(1, c), m_stack_list)));
            break;

        case sexp_t::STRING_STACK:
        {
            if( c == '"' )
            {
                m_stack[m_stack.size() - 2]->add(m_stack.back());
                m_stack.pop_back();
                if( m_stack.back()->child(0).type() == sexp_t::TUPLE_STACK and
                    m_stack.back()->child(0).child(0).m_str == "quote" )
                {
                    m_stack[m_stack.size() - 2]->add(m_stack.back());
                    m_stack.pop_back();
                }
            }
            else if( c == '\\' ) m_stack.back()->m_str += m_stream.get();
            else if( c != ';'  ) m_stack.back()->m_str += c;
            break;
        }
        case sexp_t::TUPLE_STACK:
        {
            if( is_sexp_separator(c) )
            {
                sexp_t *p_atom = m_stack.back();
                m_stack.pop_back();
                m_stack.back()->add(p_atom);
                if( m_stack.back()->child(0).type() == sexp_t::TUPLE_STACK and
                    m_stack.back()->child(0).child(0).m_str == "quote" )
                {
                    m_stack[m_stack.size() - 2]->add(m_stack.back());
                    m_stack.pop_back();
                }
                m_stream.unget();
            }
            else if( c == '\\' )
                m_stack.back()->child(0).m_str += m_stream.get();
            else
                m_stack.back()->child(0).m_str += c;
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

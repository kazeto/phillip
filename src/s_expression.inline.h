#pragma once

namespace phil
{

namespace sexp
{


inline stack_t::stack_t(
    stack_type_e _type, const std::string& e, std::list<stack_t> &stack_list )
    : type(_type)
{
    if( type == TUPLE_STACK )
    {
        stack_list.push_back( stack_t( STRING_STACK, e, stack_list ) );
        children.push_back( &(stack_list.back()) );
    }
    else
        str = e;
}


inline std::string stack_t::to_string() const
{
    std::string exp;
    print( &exp );
    return exp;
}


inline bool stack_t::is_functor( const std::string &func_name ) const
{
    if( children.size() <= 1 )              return false;
    if( children[0]->children.size() == 0 ) return false;
    return func_name.empty() ? true : (children[0]->children[0]->str == func_name);
}


inline bool stack_t::is_parameter() const
{
    std::string str = get_string();
    if( str.empty() ) return false;
    else              return ( get_string().at(0) == ':' );
}


inline std::string stack_t::get_string() const
{
    if( type == STRING_STACK )
        return str;
    else if( children.size() == 1 and children[0]->type == STRING_STACK )
        return children[0]->str;
    else
        return "";
}


inline reader_t::reader_t( std::istream &_stream, const std::string &name )
    : m_line_num(1), m_read_bytes(0), m_stream(_stream), m_name(name)
{
    m_stack.push_back( new_stack( stack_t(stack_t::LIST_STACK) ) );
    read();
};


inline const std::deque<stack_t*>& reader_t::get_queue() const
{ return m_stack; }


inline const std::list<stack_t>& reader_t::get_list() const
{ return m_stack_list; }


inline size_t reader_t::get_read_bytes() const
{ return m_read_bytes; }


inline void reader_t::clear_stack()
{
    m_stack_list.clear();
    m_stack.clear();
    m_stack.push_back( new_stack( stack_t(stack_t::LIST_STACK) ) );
}


inline void reader_t::clear_latest_stack(int n)
{
    for( int i=0; i<n; ++i )
        m_stack_list.pop_back();
}


inline stack_t* reader_t::new_stack( const stack_t &ss )
{
    m_stack_list.push_back(ss);
    return &(m_stack_list.back());
}


inline bool reader_t::is_sexp_separator( char c )
{
    return c == '('
        or c == ')'
        or c == '"'
        or c == ' '
        or c == '\t'
        or c == '\n'
        or c == '\r';
}


}

}



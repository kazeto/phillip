#pragma once

#include <fstream>
#include <string>
#include <list>
#include <deque>
#include <vector>
#include <ciso646>

#include "./define.h"


namespace phil
{

namespace sexp
{


class reader_t;


/** A class of stack of s-expression. */
class sexp_t
{    
    friend reader_t;
public:
    enum stack_type_e { LIST_STACK, STRING_STACK, TUPLE_STACK };
      
    inline sexp_t() : m_type(LIST_STACK) {}
    inline sexp_t(stack_type_e t) : m_type(t) {}
    inline sexp_t(stack_type_e t, const std::string &e, std::list<sexp_t> &stacks);

    stack_type_e type() const { return m_type; }
    const std::vector<sexp_t*>& children() const { return m_children; }
    const sexp_t& child(size_t i) const { return *m_children.at(i); }

    int find_functor(const std::string &func_name) const;
    
    inline bool is_functor(const std::string &func_name = "") const;
    inline bool is_parameter() const;
    
    inline string_t string() const; /** Get str of this or children. */

    string_t expr() const;

private:
    sexp_t& child(size_t i) { return *m_children.at(i); }
    void add(sexp_t* p) { m_children.push_back(p); }

    stack_type_e m_type;
    std::vector<sexp_t*> m_children;
    string_t m_str; /// Content of string-stack instance.
};


/** reader of s-expression. */
class reader_t
{  
public:
    inline reader_t(std::istream &_stream, const std::string &name = "");
    inline ~reader_t() { clear_stack(); }
    
    /** Read and parse s-expression.  */
    reader_t& read();
    
    inline const std::deque<sexp_t*> &get_queue() const { return m_stack; }
    inline const std::list<sexp_t>   &get_list()  const { return m_stack_list; }
    inline const sexp_t* get_stack() const { return m_stack_current; }
    inline size_t get_read_bytes() const;
    inline size_t get_line_num() const { return m_line_num; }

    inline const std::string& name() const { return m_name; }
      
    inline bool is_end()  const { return m_stream.eof(); }
    inline bool is_root() const { return m_stack.size() == 1; }
    
    inline void clear_stack();
    inline void clear_latest_stack(int n);
        
private:
    inline static bool is_sexp_separator(char c);

    /** Add a new stack and return the pointer of the added stack. */
    inline sexp_t* new_stack(const sexp_t &ss);
    
    std::istream         &m_stream;
    std::string m_name;

    std::deque<sexp_t*>  m_stack;
    std::list<sexp_t>    m_stack_list;

    sexp_t *m_stack_current;

    size_t   m_line_num;
    size_t   m_read_bytes;
};



/* -------- inline methods -------- */


inline sexp_t::sexp_t(
    stack_type_e _type, const std::string& e, std::list<sexp_t> &stack_list)
    : m_type(_type)
{
    if (m_type == TUPLE_STACK)
    {
        stack_list.push_back(sexp_t(STRING_STACK, e, stack_list));
        m_children.push_back(&(stack_list.back()));
    }
    else
        m_str = e;
}


inline bool sexp_t::is_functor(const std::string &func_name) const
{
    if (m_children.size() <= 1)            return false;
    if (m_children[0]->children().empty()) return false;
    return func_name.empty() ? true : (m_children.front()->string() == func_name);
}


inline bool sexp_t::is_parameter() const
{
    const auto &str = string();
    return str.empty() ? false : (str.front() == ':');
}


inline string_t sexp_t::string() const
{
    if (m_type == STRING_STACK)
        return m_str;
    else if (m_children.size() == 1)
        return m_children.front()->string();
    else
        return "";
}


inline reader_t::reader_t(std::istream &_stream, const std::string &name)
: m_line_num(1), m_read_bytes(0), m_stream(_stream), m_name(name),
m_stack_current(nullptr)
{
    m_stack.push_back(new_stack(sexp_t(sexp_t::LIST_STACK)));
    read();
};


inline size_t reader_t::get_read_bytes() const
{
    return m_read_bytes;
}


inline void reader_t::clear_stack()
{
    m_stack_list.clear();
    m_stack.clear();
    m_stack.push_back(new_stack(sexp_t(sexp_t::LIST_STACK)));
}


inline void reader_t::clear_latest_stack(int n)
{
    for (int i = 0; i < n; ++i)
        m_stack_list.pop_back();
}


inline sexp_t* reader_t::new_stack(const sexp_t &ss)
{
    m_stack_list.push_back(ss);
    return &(m_stack_list.back());
}


inline bool reader_t::is_sexp_separator(char c)
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




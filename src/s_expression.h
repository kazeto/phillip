#pragma once

#include <fstream>
#include <string>
#include <list>
#include <deque>
#include <ciso646>


namespace phil
{

namespace sexp
{


/** A class of stack of s-expression. */
class stack_t
{    
public:
    enum stack_type_e { LIST_STACK, STRING_STACK, TUPLE_STACK };
    
    stack_type_e type;
    std::deque<stack_t*> children;
    std::string str; /**< Content of string-stack instance. */
  
    inline stack_t() : type(LIST_STACK) {}
    inline stack_t( stack_type_e t ) : type(t) {}
    inline stack_t( stack_type_e t,
                    const std::string &e,
                    std::list<stack_t> &stack_list );

    int find_functor( const std::string &func_name ) const;
    
    inline bool is_functor( const std::string &func_name="" ) const;
    inline bool is_parameter() const;
    
    inline std::string get_string() const; /** Get str of this or children. */
    inline std::string to_string() const;  /**< Get string-expression this. */
    
    void print( std::string *p_out_str ) const;
};


/** reader of s-expression. */
class reader_t
{  
public:
    inline reader_t( std::istream &_stream, const std::string &name="" );
    inline ~reader_t() { clear_stack(); }
    
    /** Read and parse s-expression.  */
    reader_t& read();
    
    inline const std::deque<stack_t*> &get_queue() const;
    inline const std::list<stack_t>   &get_list()  const;
    inline const stack_t* get_stack() const { return m_stack_current; }
    inline size_t get_read_bytes() const;
    inline size_t get_line_num() const { return m_line_num; }

    inline const std::string& name() const { return m_name; }
      
    inline bool is_end()  const { return m_stream.eof(); }
    inline bool is_root() const { return m_stack.size() == 1; }
    
    inline void clear_stack();
    inline void clear_latest_stack(int n);
        
private:
    inline static bool is_sexp_separator( char c );

    /** Add a new stack and return the pointer of the added stack. */
    inline stack_t* new_stack( const stack_t &ss );
    
    std::istream         &m_stream;
    std::deque<stack_t*>  m_stack;
    std::list<stack_t>    m_stack_list;
    std::string m_name;
    stack_t *m_stack_current;
    size_t   m_line_num;
    size_t   m_read_bytes;
};


}

}


#include "./s_expression.inline.h"



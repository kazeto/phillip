/* -*- coding: utf-8 -*- */

#ifndef HENRY_PROCESSOR_H
#define HENRY_PROCESSOR_H


#include "./s_expression.h"
#include "./logical_function.h"

namespace phil
{

namespace proc
{


/** A virtual class of component of processor_t. */
class component_t
{
public:
    virtual void prepare() = 0;
    virtual void process( const sexp::reader_t* ) = 0;
    virtual void quit() = 0;
};


/** A class of component for parsing input to observations. */
class parse_obs_t : public component_t
{
public:
    parse_obs_t( std::vector<lf::input_t> *ipt ) : m_inputs(ipt) {}
    virtual void prepare() {}
    virtual void process( const sexp::reader_t* );
    virtual void quit() {}

private:
    std::vector<lf::input_t> *m_inputs;
};


/** A class of component for compiling knowledge base. */
class compile_kb_t : public component_t
{
public:
    compile_kb_t(kb::knowledge_base_t *out_ptr, bool is_temporary = false)
        : m_kb(out_ptr), m_is_temporary(is_temporary) {}
    virtual void prepare();
    virtual void process( const sexp::reader_t* );
    virtual void quit();
    
private:
    kb::knowledge_base_t *m_kb;
    bool m_is_temporary;
};


/** A class to process input. */
class processor_t
{
public:
    processor_t() : m_recursion(0) {};
    ~processor_t();

    /** Process inputs.
     *  Standard input is regarded as input when inputs is empty.
     *  @param inputs List of filename of input file. */
    void process( std::vector<std::string> inputs);
    void add_component( component_t* c ) { m_components.push_back(c); }
    
private:
    enum processing_state_e { STATE_INIT, STATE_PREPARED, STATE_QUITED };
    
    void include( const sexp::reader_t* );

    int m_recursion;
    std::list<component_t*> m_components;
};


}

}



#endif

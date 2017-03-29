/* -*- coding: utf-8 -*- */

#pragma once

#include "./sexp.h"
#include "./logical_function.h"


namespace phil
{

namespace proc
{


/** A virtual class of component of processor_t. */
class component_t
{
public:
    component_t(bool do_skip = false) : m_do_skip_parse_error(do_skip) {}
    
    virtual void prepare() = 0;
    virtual void process(const sexp::reader_t*) = 0;
    virtual void quit() = 0;

protected:
    void print_syntax_error(const sexp::reader_t *r, const std::string &m);
    
    /** If true, this will skip invalid inputs.
     *  Otherwise, this will abort Phillip when it encounts an invalid input. */
    bool m_do_skip_parse_error;
};


/** A class of component for parsing input to observations. */
class parse_obs_t : public component_t
{
public:
    parse_obs_t(std::vector<lf::input_t> *ipt, bool do_skip_parse_error)
        : component_t(do_skip_parse_error), m_inputs(ipt) {}
    
    virtual void prepare() {}
    virtual void process(const sexp::reader_t*);
    virtual void quit() {}

private:
    std::vector<lf::input_t> *m_inputs;
};


/** A class of component for compiling knowledge base. */
class compile_kb_t : public component_t
{
public:
    compile_kb_t(bool do_skip_parse_error)
        : component_t(do_skip_parse_error) {}
    
    virtual void prepare();
    virtual void process(const sexp::reader_t*);
    virtual void quit();
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



/* -*- coding: utf-8 -*- */

#include <algorithm>

#include "./processor.h"
#include "./phillip.h"


namespace phil
{

namespace proc
{


#ifdef SKIP_SYNTAX_ERROR

#define _assert_syntax(x, s, e) \
    if (not(x)){ \
        util::print_warning(\
            util::format("Syntax error at line %d: ", s.get_line_num()) \
            + e + "\n" + s.get_stack()->expr()); \
        return; }

#else

#define _assert_syntax(x, s, e) \
    if (not(x)) throw phillip_exception_t(\
    util::format("Syntax error at line %d: ", s.get_line_num()) \
    + e + "\n" + s.get_stack()->expr());

#endif


void parse_obs_t::process(const sexp::reader_t *reader)
{
    const sexp::sexp_t& stack(*reader->get_stack());

    if (not stack.is_functor("O") or m_inputs == NULL)
        return;

    /* SHOULD BE ROOT. */
    _assert_syntax(reader->is_root(), (*reader), "Function O should be root.");

    std::string name = "?";
    int i_obs = stack.find_functor(lf::OPR_STR_AND);
    int i_req = stack.find_functor(lf::OPR_STR_REQUIREMENT);
    int i_name = stack.find_functor(lf::OPR_STR_NAME);

    if (i_name >= 0)
        name = stack.child(i_name).child(1).string();

    _assert_syntax((i_obs >= 0), (*reader), "Any input was not found: " + name);

    lf::input_t data;
    data.name = reader->name() + "::" + name;
    data.obs = lf::logical_function_t(stack.child(i_obs));

    _assert_syntax(data.obs.is_valid_as_observation(), (*reader), "Invalid observation: \"" + name + "\"");

    if (i_req >= 0)
    {
        data.req = lf::logical_function_t(stack.child(i_req));
        _assert_syntax(data.req.is_valid_as_requirements(), (*reader), "Arguments of req are invalid.");
    }

    m_inputs->push_back(data);
}


void compile_kb_t::prepare()
{}


void compile_kb_t::process( const sexp::reader_t *reader )
{    
    const sexp::sexp_t *stack(reader->get_stack());

    if (not stack->is_functor("B")) return;

    /* SHOULD BE ROOT. */
    _assert_syntax(reader->is_root(), (*reader), "Function B should be root.");

    index_t idx_name = stack->find_functor(lf::OPR_STR_NAME);        
    std::string name((idx_name >= 0) ? stack->child(idx_name).child(1).string() : "");

    for (const auto& c : stack->children())
    {
        if (c->is_functor(lf::OPR_STR_IMPLICATION))
            kb::kb()->axioms.add(lf::logical_function_t{ *c }, name);
        else if (c->is_functor(lf::OPR_STR_INCONSISTENT))
            kb::kb()->predicates.define_mutual_exclusion(lf::logical_function_t(*c));
        else if (c->is_functor(lf::OPR_STR_DEFINE))
            kb::kb()->predicates.define_functional_predicate(lf::logical_function_t(*c));
    }
}


void compile_kb_t::quit()
{}


processor_t::~processor_t()
{
    for( auto it=m_components.begin(); it!=m_components.end(); ++it )
        delete (*it);
}


void processor_t::process( std::vector<std::string> inputs )
{
    if( inputs.empty() )
        inputs.push_back( "-" );

    IF_VERBOSE_FULL(
        "processor_t::process: inputs={" +
        util::join(inputs.begin(), inputs.end(), ", ") + "}" );

    if( m_recursion++ == 0 )
    for (auto it = m_components.begin(); it != m_components.end(); ++it)
        (*it)->prepare();
            
    for (int i = 0; i < inputs.size(); ++i)
    {
        std::istream *p_is( &std::cin );
        std::ifstream file;
        size_t file_size(0);
        const std::string &input_path(inputs.at(i));
        std::string filename;

        IF_VERBOSE_1(
            util::format("Reading input #%d: \"%s\"", i, input_path.c_str()));
        
        if( input_path != "-" )
        {
            file.open( input_path.c_str() );
            p_is = &file;

            if (file.fail())
                throw phillip_exception_t("File not found: " + input_path);

            file_size = util::get_file_size(*p_is);
            filename  = input_path.substr( input_path.rfind('/')+1 );
        }
        else
            filename = "stdin";
        
        sexp::reader_t reader( *p_is, filename );
        hash_set<long> notified;
                
        for( ; not reader.is_end(); reader.read() )
        {
            if (file_size != 0 and is_verbose(VERBOSE_4))
            {
                size_t read_bytes(reader.get_read_bytes());
                int progress(100 * read_bytes / file_size);
                if (notified.count(progress) == 0)
                {
                    notified.insert(progress);
                    std::cerr
                        << util::time_stamp()
                        << input_path << ":" << read_bytes
                        << "/" << file_size
                        << " bytes processed (" << progress << "%)."
                        << std::endl;
                }
            }

            for( auto it=m_components.begin(); it!=m_components.end(); ++it )
                (*it)->process( &reader );
            
            include( &reader );
        }

        if( input_path != "-" ) file.close();

        if( reader.get_queue().size() != 1 )
        {
            std::string out = util::format(
                "Syntax error: too few parentheses. Around here, or line %d"
                " (typically the expression followed by this): %s",
                reader.get_line_num(),
                reader.get_stack()->expr().c_str() );
            throw phillip_exception_t(out);
        }
    }
    
    if( --m_recursion == 0 )
        for( auto it=m_components.begin(); it!=m_components.end(); ++it )
            (*it)->quit();
}


void processor_t::include( const sexp::reader_t *reader )
{
    const sexp::sexp_t& stack( *reader->get_stack() );
    if( stack.is_functor("include") )
    {
        const sexp::sexp_t& arg(stack.child(1));
        _assert_syntax(
            (arg.type() == sexp::sexp_t::STRING_STACK),
            (*reader), "what is included should be a string.");
        
        std::vector<std::string> inputs(1, arg.string());
        process( inputs );
    }
}



}

}

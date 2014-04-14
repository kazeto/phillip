/* -*- coding: utf-8 -*- */

#include "./processor.h"
#include "./phillip.h"


namespace phil
{

namespace proc
{


#define _assert_syntax(x, s, e) \
    if( not x ){ \
        print_error( \
            format("Syntax error at line %d:", s.get_line_num()) \
            + e + "\n" + s.get_stack()->to_string()); \
        throw; }


void parse_obs_t::process( const sexp::reader_t *reader )
{
    const sexp::stack_t& stack( *reader->get_stack() );
    
    if( not stack.is_functor("O") or m_inputs == NULL )
        return;

    /* SHOULD BE ROOT. */
    _assert_syntax(
        reader->is_root(), (*reader), "Function O should be root." );

    std::string name = "?";
    int i_x = stack.find_functor("^");
    int i_y = stack.find_functor("label");
    int i_name = stack.find_functor("name");
    
    if( i_name >= 0 )
        name = stack.children.at(i_name)->children.at(1)->get_string();
    if( i_x < 0 )
    {
        print_warning("Input not found:" + name);
        return;
    }

    lf::input_t data;
    data.name = reader->name() + "::" + name;
    data.obs  = lf::logical_function_t( *stack.children.at(i_x) );
        
    if( i_y >= 0 )
        data.label = lf::logical_function_t( *stack.children.at(i_y) );
        
    m_inputs->push_back( data );
}


void compile_kb_t::prepare()
{}


void compile_kb_t::process( const sexp::reader_t *reader )
{    
    const sexp::stack_t *stack( reader->get_stack() );

    if( not stack->is_functor("B") or m_kb == NULL )
        return;

    /* SHOULD BE ROOT. */
    _assert_syntax(
        reader->is_root(), (*reader), "Function B should be root." );
        
    /* IDENTIFY THE LOGICAL FORM PART. */
    int idx_lf = stack->find_functor("=>");
    int idx_inc = stack->find_functor("xor");
    int idx_name = stack->find_functor("name");
        
    _assert_syntax(
        (idx_lf != -1 or idx_inc != -1), (*reader),
        "no logical connectors found." );

    std::string name;
    if (idx_name >= 0)
        name = stack->children.at(idx_name)->children.at(1)->get_string();
        
    if (idx_lf >= 0 or idx_inc >= 0)
    {
        int idx = (idx_lf >= 0) ? idx_lf : idx_inc;
        _assert_syntax(
            (stack->children.at(idx)->children.size() >= 3), (*reader),
            "function '=>' and '_|_' takes two arguments.");

        lf::logical_function_t lf(*stack->children[idx]);
        if (idx_lf >= 0)
        {
            IF_VERBOSE_FULL("add implication: " + stack->to_string());

            m_is_temporary ?
                m_kb->insert_implication_temporary(lf, name) :
                m_kb->insert_implication_for_compile(lf, name);
        }
        else if (idx_inc >= 0)
        {
            IF_VERBOSE_FULL("add inconsistency: " + stack->to_string());

            m_is_temporary ?
                m_kb->insert_inconsistency_temporary(lf, name) :
                m_kb->insert_inconsistency_for_compile(lf, name);
        }
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
        join(inputs.begin(), inputs.end(), ", ") + "}" );

    if( m_recursion++ == 0 )
        for( auto it=m_components.begin(); it!=m_components.end(); ++it )
            (*it)->prepare();
            
    for( auto it=inputs.begin(); it!=inputs.end(); ++it )
    {
        std::istream *p_is( &std::cin );
        std::ifstream file;
        size_t file_size(0);
        const std::string &input_path( *it );
        std::string filename;
        
        if( input_path != "-" )
        {
            file.open( input_path.c_str() );
            p_is = &file;
            if( file.fail() )
            {
                print_error( "File not found: " + input_path );
                break;
            }
            file_size = get_file_size(*p_is);
            filename  = input_path.substr( input_path.rfind('/')+1 );
        }
        else
            filename = "stdin";
        
        sexp::reader_t reader( *p_is, filename );
        hash_set<long> notified;
                
        for( ; not reader.is_end(); reader.read() )
        {
            if( file_size != 0 )
            {
                size_t read_bytes(reader.get_read_bytes());
                int progress( 100 * read_bytes / file_size );
                if( notified.count(progress) == 0 )
                {
                    notified.insert(progress);
                    std::cerr << time_stamp()
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
            std::string out = format(
                "Syntax error: too few parentheses. Around here, or line %d"
                " (typically the expression followed by this):\n%s",
                reader.get_line_num(),
                reader.get_stack()->to_string().c_str() );
            print_error( out );
        }
    }
    
    if( --m_recursion == 0 )
        for( auto it=m_components.begin(); it!=m_components.end(); ++it )
            (*it)->quit();
}


void processor_t::include( const sexp::reader_t *reader )
{
    const sexp::stack_t& stack( *reader->get_stack() );
    if( stack.is_functor("include") )
    {
        const sexp::stack_t& arg( *stack.children.at(1) );
        _assert_syntax(
            (arg.type == sexp::stack_t::STRING_STACK),
            (*reader), "what is included should be a string.");
        
        std::vector<std::string> inputs( 1, arg.get_string() );
        process( inputs );
    }
}



}

}

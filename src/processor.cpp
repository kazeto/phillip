/* -*- coding: utf-8 -*- */

#include <algorithm>

#include "./processor.h"
#include "./phillip.h"


namespace phil
{

namespace proc
{


void component_t::print_syntax_error(const sexp::reader_t *r, const std::string &m)
{
    std::string disp =
        util::format("Syntax error at line %d: ", r->get_line_num()) +
        + m + "\n" + r->get_stack()->expr();
        
    if (m_do_skip_parse_error)
        util::print_warning(disp);
    else
        throw phillip_exception_t(disp);
}


void parse_obs_t::process(const sexp::reader_t *reader)
{
    const sexp::sexp_t& stack(*reader->get_stack());
    std::string message; // FOR ERROR MESSAGE

    if (not stack.is_functor("O") or m_inputs == NULL)
        return;

    /* SHOULD BE ROOT. */
    if (not reader->is_root())
    {
        print_syntax_error(reader, "Function O should be root.");
        return;
    }

    std::string name = "?";
    int i_obs = stack.find_functor(lf::OPR_STR_AND);
    int i_req = stack.find_functor(lf::OPR_STR_REQUIREMENT);
    int i_name = stack.find_functor(lf::OPR_STR_NAME);

    if (i_name >= 0)
        name = stack.child(i_name).child(1).string();

    if (i_obs < 0)
    {
        print_syntax_error(reader, "Any observation was not found.");
        return;
    }

    lf::input_t data;
    data.name = reader->name() + "::" + name;
    data.obs = lf::logical_function_t(stack.child(i_obs));

    if (not data.obs.is_valid_as_observation(&message))
    {
        print_syntax_error(reader, message);
        return;
    }

    if (i_req >= 0)
    {
        data.req = lf::logical_function_t(stack.child(i_req));
        if(not data.req.is_valid_as_requirements(&message))
        {
            print_syntax_error(reader, message);
            return;
        }
    }

    m_inputs->push_back(data);
}


void compile_kb_t::prepare()
{}


void compile_kb_t::process(const sexp::reader_t *reader)
{
    const sexp::sexp_t *stack(reader->get_stack());
    std::string message; // FOR ERROR MESSAGE

    if (not stack->is_functor("B")) return;

    /* SHOULD BE ROOT. */
    if (not reader->is_root())
    {
        print_syntax_error(reader, "Function B must be root.");
        return;
    }

    index_t idx_name = stack->find_functor(lf::OPR_STR_NAME);        
    std::string name =
        (idx_name >= 0) ? stack->child(idx_name).child(1).string() : "";
    
    for (const auto& c : stack->children())
    {
        if (c->is_functor(lf::OPR_STR_IMPLICATION))
        {
            lf::logical_function_t func{ *c };
            if (func.is_valid_as_implication(&message))
                kb::kb()->axioms.add(func, name);
            else
                print_syntax_error(reader, message);
        }
        else if (c->is_functor(lf::OPR_STR_INCONSISTENT))
        {
            lf::logical_function_t func{ *c };
            if (func.is_valid_as_inconsistency(&message))
                kb::kb()->predicates.define_mutual_exclusion(func);
            else
                print_syntax_error(reader, message);
        }
        else if (c->is_functor(lf::OPR_STR_DEFINE))
        {
            lf::logical_function_t func{ *c };
            if (func.is_valid_as_definition(&message))
                kb::kb()->predicates.define_functional_predicate(func);
            else
                print_syntax_error(reader, message);
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

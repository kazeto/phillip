/* -*- coding: utf-8 -*- */

#include <algorithm>

#include "./processor.h"
#include "./phillip.h"


namespace phil
{

    namespace proc
    {


#define _assert_syntax(x, s, e) \
        if (not(x)) throw phillip_exception_t( \
        util::format("Syntax error at line %d:", s.get_line_num()) \
        + e + "\n" + s.get_stack()->to_string()); \


void parse_obs_t::process(const sexp::reader_t *reader)
{
    const sexp::stack_t& stack(*reader->get_stack());

    if (not stack.is_functor("O") or m_inputs == NULL)
        return;

    /* SHOULD BE ROOT. */
    _assert_syntax(reader->is_root(), (*reader), "Function O should be root.");

    std::string name = "?";
    int i_obs = stack.find_functor(lf::OPR_STR_AND);
    int i_req = stack.find_functor(lf::OPR_STR_REQUIREMENT);
    int i_name = stack.find_functor(lf::OPR_STR_NAME);

    if (i_name >= 0)
        name = stack.children.at(i_name)->children.at(1)->get_string();

    _assert_syntax((i_obs >= 0), (*reader), "Any input was not found:" + name);

    lf::input_t data;
    data.name = reader->name() + "::" + name;
    data.obs = lf::logical_function_t(*stack.children.at(i_obs));

    if (i_req >= 0)
    {
        data.req = lf::logical_function_t(*stack.children.at(i_req));
        _assert_syntax(data.req.is_valid_as_requirements(), (*reader), "Arguments of req are invalid.");
    }

    m_inputs->push_back(data);
}


void compile_kb_t::prepare()
{}


void compile_kb_t::process( const sexp::reader_t *reader )
{    
    const sexp::stack_t *stack(reader->get_stack());
    kb::knowledge_base_t *_kb = kb::knowledge_base_t::instance();

    if (stack->is_functor("ASSERT"))
    {
        _assert_syntax(reader->is_root(), (*reader), "Function ASSERT should be root.");
        _assert_syntax(stack->children.size() >= 1, (*reader), "There is no operator of assertion.");
        _assert_syntax(stack->children.size() >= 2, (*reader), "There is no argument of assertion.");

        if (stack->children.at(0)->get_string() == "stopword")
        {
            for (int i = 1; i < stack->children.size(); ++i)
            {
                arity_t a = stack->children.at(i)->get_string();
                kb::kb()->assert_stop_word(a);
                IF_VERBOSE_FULL("Added stop-word assertion: " + a);
            }
        }
    }
    
    if (not stack->is_functor("B"))
        return;

    /* SHOULD BE ROOT. */
    _assert_syntax(reader->is_root(), (*reader), "Function B should be root.");

    /* IDENTIFY THE LOGICAL FORM PART. */
    index_t idx_lf = stack->find_functor(lf::OPR_STR_IMPLICATION);
    index_t idx_para = stack->find_functor(lf::OPR_STR_PARAPHRASE);
    index_t idx_inc = stack->find_functor(lf::OPR_STR_INCONSISTENT);
    index_t idx_pp = stack->find_functor(lf::OPR_STR_UNIPP);
    index_t idx_as = stack->find_functor(lf::OPR_STR_EXARGSET);
    index_t idx_name = stack->find_functor(lf::OPR_STR_NAME);
    index_t idx_assert = stack->find_functor(lf::OPR_STR_ASSERTION);
        
    _assert_syntax(
        (idx_lf >= 0 or idx_para >= 0 or idx_inc >= 0 or idx_pp >= 0 or idx_as >= 0 or idx_assert >= 0),
        (*reader), "No logical connectors found." );

    std::string name;
    if (idx_name >= 0)
        name = stack->children.at(idx_name)->children.at(1)->get_string();

    if (idx_lf >= 0 or idx_para >= 0)
    {
        index_t idx = std::max(idx_lf, idx_para);
        lf::logical_function_t func(*stack->children[idx]);
        _assert_syntax(
            (stack->children.at(idx)->children.size() >= 3), (*reader),
            "Function '=>' and '<=>' takes two arguments.");
        IF_VERBOSE_FULL(
            ((idx_lf >= 0) ? "Added implication: " : "Added paraphrase") +
            stack->to_string());
        _kb->insert_implication(func, name);
    }
    else if (idx_inc >= 0)
    {
        lf::logical_function_t func(*stack->children[idx_inc]);
        _assert_syntax(
            (stack->children.at(idx_inc)->children.size() >= 3), (*reader),
            "Function 'xor' takes two arguments.");
        IF_VERBOSE_FULL("Added inconsistency: " + stack->to_string());
        _kb->insert_inconsistency(func);
    }
    else if (idx_pp >= 0)
    {
        lf::logical_function_t func(*stack->children[idx_pp]);
        _assert_syntax(
            (stack->children.at(idx_pp)->children.size() >= 2), (*reader),
            "Function 'unipp' takes one argument.");
        IF_VERBOSE_FULL("Added unification-postponement: " + stack->to_string());
        _kb->insert_unification_postponement(func);
    }
    else if (idx_as >= 0)
    {
        lf::logical_function_t func(*stack->children[idx_as]);
        if (phillip_main_t::verbose() == FULL_VERBOSE)
        {
            const std::vector<term_t> &terms = func.literal().terms;
            std::string disp;
            for (auto it = terms.begin(); it != terms.end(); ++it)
                disp += (it != terms.begin() ? ", " : "") + it->string();
            util::print_console("Added argument-set: {" + disp + "}");
        }
        _kb->insert_argument_set(func);
    }
    else if (idx_assert >= 0)
    {
        const sexp::stack_t *target = stack->children.at(idx_assert);
        _assert_syntax(target->children.size() > 1, (*reader), "Function 'assert' takes one operator.");

        if (target->children.at(1)->get_string() == "stopword")
        {
            _assert_syntax(
                target->children.size() > 2, (*reader),
                "Function 'assert stopword' takes at least one argument.");
            for (int i = 2; i < target->children.size(); ++i)
            {
                arity_t a = target->children.at(i)->get_string();
                kb::kb()->assert_stop_word(a);
                IF_VERBOSE_FULL("Added stop-word assertion: " + a);
            }
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
            if( file_size != 0 )
            {
                size_t read_bytes(reader.get_read_bytes());
                int progress( 100 * read_bytes / file_size );
                if( notified.count(progress) == 0 )
                {
                    notified.insert(progress);
                    std::cerr << util::time_stamp()
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
                reader.get_stack()->to_string().c_str() );
            throw phillip_exception_t(out);
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

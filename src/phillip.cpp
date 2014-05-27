/* -*- coding: utf-8 -*- */


#include <ctime>
#include "./phillip.h"


namespace phil
{


phillip_main_t *phillip_main_t::ms_instance = NULL;


phillip_main_t *phillip_main_t::get_instance()
{
    static phillip_main_t singleton;
    return &singleton;
}


phillip_main_t::phillip_main_t()
: m_lhs_enumerator(NULL), m_ilp_convertor(NULL), m_ilp_solver(NULL),
m_kb(NULL), m_input(NULL), m_lhs(NULL), m_ilp(NULL),
m_timeout(-1), m_verboseness(0), m_is_debugging(false),
m_clock_for_enumerate(0), m_clock_for_convert(0),
m_clock_for_solve(0), m_clock_for_infer(0)
{}


phillip_main_t::~phillip_main_t()
{
    if (m_lhs_enumerator != NULL) delete m_lhs_enumerator;
    if (m_ilp_convertor != NULL)  delete m_ilp_convertor;
    if (m_ilp_solver != NULL)     delete m_ilp_solver;
    if (m_kb != NULL) delete m_kb;

    if (m_input != NULL) delete m_input;
    if (m_lhs != NULL)   delete m_lhs;
    if (m_ilp != NULL)   delete m_ilp;
}


std::ofstream* _open_file(const std::string &path, std::ios::openmode mode)
{
    if (not path.empty())
    {
        std::ofstream *fo = new std::ofstream(path.c_str(), mode);
        if (fo->good())
            return fo;
        else
        {
            print_error_fmt("Cannot open file: \"%s\"", path.c_str());
            delete fo;
        }
    }
    return NULL;
}


void phillip_main_t::infer(const std::vector<lf::input_t> &inputs, size_t idx)
{
    if( not can_infer() )
    {
        print_error("Henry cannot infer!!");
        if (m_lhs_enumerator == NULL)
            print_error("    - No lhs_enumerator!");
        if (m_ilp_convertor == NULL)
            print_error("    - No ilp_convertor!");
        if (m_ilp_solver == NULL)
            print_error("    - No ilp_solver!");
        if (m_kb == NULL)
            print_error("    - No knowledge_base!");

        return;
    }
    
    bool is_begin(idx == 0), is_end(idx == inputs.size() - 1);
    std::ios::openmode mode =
        std::ios::out | (is_begin ? std::ios::trunc : std::ios::app);
    std::ofstream *fo(NULL);

    reset_for_inference();
    m_input = new lf::input_t(inputs.at(idx));

    clock_t begin_infer(clock());
    
    IF_VERBOSE_2("Generating latent-hypotheses-set...");
    clock_t begin_flhs(clock());
    m_lhs = m_lhs_enumerator->execute();
    clock_t end_flhs(clock());
    m_clock_for_enumerate += end_flhs - begin_flhs;
    IF_VERBOSE_2(
        m_lhs->is_timeout() ?
        "Interrupted generating latent-hypotheses-set." :
        "Completed generating latent-hypotheses-set.");

    if ((fo = _open_file(param("path_lhs_out"), mode)) != NULL)
    {
        if (is_begin)
        {
            (*fo) << "<phillip>" << std::endl;
            write_configure(fo);
        }
        m_lhs->print(fo);
        if (is_end) (*fo) << "</phillip>" << std::endl;
        delete fo;
    }

    IF_VERBOSE_2("Converting LHS into linear-programming-problems...");
    clock_t begin_flpp(clock());
    m_ilp = m_ilp_convertor->execute();
    clock_t end_flpp(clock());
    m_clock_for_convert += end_flpp - begin_flpp;
    IF_VERBOSE_2("Completed convertion into linear-programming-problems...");

    if ((fo = _open_file(param("path_ilp_out"), mode)) != NULL)
    {
        if (is_begin)
        {
            (*fo) << "<phillip>" << std::endl;
            write_configure(fo);
        }
        m_ilp->print(fo);
        if (is_end) (*fo) << "</phillip>" << std::endl;
        delete fo;
    }

    IF_VERBOSE_2("Solving...");
    clock_t begin_fsol(clock());
    m_ilp_solver->execute(&m_sol);
    clock_t end_fsol(clock());
    m_clock_for_solve += end_fsol - begin_fsol;
    clock_t end_infer(clock());
    m_clock_for_infer += end_infer - begin_infer;
    IF_VERBOSE_2("Completed inference.");

    for (auto sol = m_sol.begin(); sol != m_sol.end(); ++sol)
        sol->print_graph();

    if ((fo = _open_file(param("path_sol_out"), mode)) != NULL)
    {
        if (is_begin)
        {
            (*fo) << "<phillip>" << std::endl;
            write_configure(fo);
        }
        for (auto sol = m_sol.begin(); sol != m_sol.end(); ++sol)
            sol->print(fo);
        if (is_end) (*fo) << "</phillip>" << std::endl;
        delete fo;
    }

    if ((fo = _open_file(param("path_out"), mode)) != NULL)
    {
        if (is_begin)
        {
            (*fo) << "<phillip>" << std::endl;
            write_configure(fo);
        }
        for (auto sol = m_sol.begin(); sol != m_sol.end(); ++sol)
            sol->print_graph(fo);
        if (is_end) (*fo) << "</phillip>" << std::endl;
        delete fo;
    }
}


void phillip_main_t::write_configure(std::ofstream *fo) const
{
    (*fo) << "<configure>" << std::endl;

    (*fo) << "<components lhs=\"" << m_lhs_enumerator->repr()
          << "\" ilp=\"" << m_ilp_convertor->repr()
          << "\" sol=\"" << m_ilp_solver->repr()
          << "\"></components>" << std::endl;
    
    (*fo) << "<params timeout=\"" << timeout()
          << "\" verbose=\"" << verbose();
    
    for (auto it = m_params.begin(); it != m_params.end(); ++it)
        (*fo) << "\" " << it->first << "=\"" << it->second;
    
    for (auto it = m_flags.begin(); it != m_flags.end(); ++it)
        (*fo) << "\" " << (*it) << "=\"yes";
            
    (*fo) << "\"></params>" << std::endl;
    
    (*fo) << "</configure>" << std::endl;
}


}

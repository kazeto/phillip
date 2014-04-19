/* -*- coding: utf-8 -*- */


#include <ctime>
#include "./phillip.h"


namespace phil
{


phillip_main_t *phillip_main_t::ms_instance = NULL;


inline phillip_main_t::phillip_main_t()
: m_lhs_enumerator(NULL), m_ilp_convertor(NULL), m_ilp_solver(NULL),
m_kb(NULL), m_input(NULL), m_lhs(NULL), m_ilp(NULL),
m_timeout(-1), m_verboseness(0), m_is_debugging(false),
m_clock_for_enumeration(0), m_clock_for_convention(0),
m_clock_for_solution(0), m_clock_for_infer(0)
{}


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
    m_clock_for_enumeration += end_flhs - begin_flhs;
    IF_VERBOSE_2(
        m_lhs->is_timeout() ?
        "Interrupted generating latent-hypotheses-set." :
        "Completed generating latent-hypotheses-set.");

    if ((fo = _open_file(param("path_lhs_out"), mode)) != NULL)
    {
        if (is_begin) (*fo) << "<phillip>" << std::endl;
        m_lhs->print(fo);
        if (is_end) (*fo) << "</phillip>" << std::endl;
        delete fo;
    }

    IF_VERBOSE_2("Converting LHS into linear-programming-problems...");
    clock_t begin_flpp(clock());
    m_ilp = m_ilp_convertor->execute();
    clock_t end_flpp(clock());
    m_clock_for_convention += end_flpp - begin_flpp;
    IF_VERBOSE_2("Completed convertion into linear-programming-problems...");

    if ((fo = _open_file(param("path_ilp_out"), mode)) != NULL)
    {
        if (is_begin) (*fo) << "<phillip>" << std::endl;
        m_ilp->print(fo);
        if (is_end) (*fo) << "</phillip>" << std::endl;
        delete fo;
    }

    IF_VERBOSE_2("Solving...");
    clock_t begin_fsol(clock());
    m_ilp_solver->execute(&m_sol);
    clock_t end_fsol(clock());
    m_clock_for_solution += end_fsol - begin_fsol;
    IF_VERBOSE_2("Completed inference.");

    for (auto sol = m_sol.begin(); sol != m_sol.end(); ++sol)
        sol->print_graph();

    if ((fo = _open_file(param("path_sol_out"), mode)) != NULL)
    {
        if (is_begin) (*fo) << "<phillip>" << std::endl;
        for (auto sol = m_sol.begin(); sol != m_sol.end(); ++sol)
            sol->print(fo);
        if (is_end) (*fo) << "</phillip>" << std::endl;
        delete fo;
    }

    if ((fo = _open_file(param("path_out"), mode)) != NULL)
    {
        if (is_begin) (*fo) << "<phillip>" << std::endl;
        for (auto sol = m_sol.begin(); sol != m_sol.end(); ++sol)
            sol->print_graph(fo);
        if (is_end) (*fo) << "</phillip>" << std::endl;
        delete fo;
    }

    clock_t end_infer(clock());
    m_clock_for_infer += end_infer - begin_infer;

    if (flag("print_time"))
    {
        float time_lhs = (float)m_clock_for_enumeration / CLOCKS_PER_SEC;
        float time_ilp = (float)m_clock_for_convention / CLOCKS_PER_SEC;
        float time_sol = (float)m_clock_for_solution / CLOCKS_PER_SEC;
        float time_inf = (float)m_clock_for_infer / CLOCKS_PER_SEC;
        print_console("execution time:");
        print_console_fmt("    lhs: %.2f", time_lhs);
        print_console_fmt("    ilp: %.2f", time_ilp);
        print_console_fmt("    sol: %.2f", time_sol);
        print_console_fmt("    all: %.2f", time_inf);
    }
}


}

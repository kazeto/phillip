/* -*- coding: utf-8 -*- */

#include <ctime>
#include <thread>
#include <algorithm>

#include "./phillip.h"


namespace phil
{


int phillip_main_t::ms_verboseness = 0;
const std::string phillip_main_t::VERSION = "phil.2.41";


phillip_main_t::phillip_main_t()
: m_lhs_enumerator(NULL), m_ilp_convertor(NULL), m_ilp_solver(NULL),
  m_input(NULL), m_lhs(NULL), m_ilp(NULL),
  m_timeout_lhs(-1), m_timeout_ilp(-1), m_timeout_sol(-1), m_timeout_all(-1),
  m_clock_for_enumerate(0), m_clock_for_convert(0),
  m_clock_for_solve(0), m_clock_for_infer(0)
{}


phillip_main_t::~phillip_main_t()
{
    if (m_lhs_enumerator != NULL) delete m_lhs_enumerator;
    if (m_ilp_convertor != NULL)  delete m_ilp_convertor;
    if (m_ilp_solver != NULL)     delete m_ilp_solver;

    if (m_input != NULL) delete m_input;
    if (m_lhs != NULL)   delete m_lhs;
    if (m_ilp != NULL)   delete m_ilp;

    for (auto it = m_phillips_parallel.begin();
        it != m_phillips_parallel.end(); ++it)
        delete (*it);
    m_phillips_parallel.clear();
}


phillip_main_t* phillip_main_t::duplicate() const
{
    phillip_main_t *out = new phillip_main_t();
    out->set_lhs_enumerator(m_lhs_enumerator->duplicate(out));
    out->set_ilp_convertor(m_ilp_convertor->duplicate(out));
    out->set_ilp_solver(m_ilp_solver->duplicate(out));

    out->m_params.insert(m_params.begin(), m_params.end());
    out->m_flags.insert(m_flags.begin(), m_flags.end());
    out->m_timeout_lhs = m_timeout_lhs;
    out->m_timeout_ilp = m_timeout_ilp;
    out->m_timeout_sol = m_timeout_sol;

    return out;
}


std::ofstream* _open_file(const std::string &path, std::ios::openmode mode)
{
    if (not path.empty())
    {
        mkdir(get_directory_name(path));

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


void phillip_main_t::infer(const lf::input_t &input)
{
    std::ios::openmode mode = std::ios::out | std::ios::app;
    std::ofstream *fo(NULL);

    reset_for_inference();
    set_input(input);

    clock_t begin_infer(clock());
    
    execute_enumerator();
    execute_convertor();
    execute_solver();

    clock_t end_infer(clock());
    m_clock_for_infer = end_infer - begin_infer;

    if ((fo = _open_file(param("path_out"), mode)) != NULL)
    {
        for (auto sol = m_sol.begin(); sol != m_sol.end(); ++sol)
            sol->print_graph(fo);
        delete fo;
    }
}


void phillip_main_t::execute_enumerator()
{
    IF_VERBOSE_2("Generating latent-hypotheses-set...");

    if (m_lhs != NULL) delete m_lhs;

    clock_t begin_flhs(clock());
    m_lhs = m_lhs_enumerator->execute();
    clock_t end_flhs(clock());
    m_clock_for_enumerate = end_flhs - begin_flhs;

    IF_VERBOSE_2(
        m_lhs->is_timeout() ?
        "Interrupted generating latent-hypotheses-set." :
        "Completed generating latent-hypotheses-set.");

    std::ios::openmode mode = std::ios::out | std::ios::app;
    std::ofstream *fo(NULL);
    if ((fo = _open_file(param("path_lhs_out"), mode)) != NULL)
    {
        m_lhs->print(fo);
        delete fo;
    }
}


void phillip_main_t::execute_convertor()
{
    IF_VERBOSE_2("Converting LHS into linear-programming-problems...");

    clock_t begin_flpp(clock());
    m_ilp = m_ilp_convertor->execute();
    clock_t end_flpp(clock());
    m_clock_for_convert = end_flpp - begin_flpp;

    IF_VERBOSE_2(
        m_ilp->is_timeout() ?
        "Interrupted convertion into linear-programming-problems." :
        "Completed convertion into linear-programming-problems.");

    std::ios::openmode mode = std::ios::out | std::ios::app;
    std::ofstream *fo(NULL);
    if ((fo = _open_file(param("path_ilp_out"), mode)) != NULL)
    {
        m_ilp->print(fo);
        delete fo;
    }
}


void phillip_main_t::execute_solver()
{
    IF_VERBOSE_2("Solving...");

    clock_t begin_fsol(clock());
    m_ilp_solver->execute(&m_sol);
    clock_t end_fsol(clock());
    m_clock_for_solve = end_fsol - begin_fsol;

    IF_VERBOSE_2("Completed inference.");

    std::ios::openmode mode = std::ios::out | std::ios::app;
    std::ofstream *fo(NULL);
    if ((fo = _open_file(param("path_sol_out"), mode)) != NULL)
    {
        for (auto sol = m_sol.begin(); sol != m_sol.end(); ++sol)
            sol->print(fo);
        delete fo;
    }
}


void phillip_main_t::write_header() const
{
    auto write = [this](std::ostream *os)
    {
        (*os) << "<phillip>" << std::endl;
        (*os) << "<configure>" << std::endl;

        (*os) << "<version>" << VERSION << "</version>" << std::endl;

        (*os)
            << "<components lhs=\"" << m_lhs_enumerator->repr()
            << "\" ilp=\"" << m_ilp_convertor->repr()
            << "\" sol=\"" << m_ilp_solver->repr()
            << "\"></components>" << std::endl;

        const kb::knowledge_base_t *base = kb::knowledge_base_t::instance();
        (*os)
            << "<knowledge_base path=\"" << base->filename()
            << "\" size=\"" << base->num_of_axioms()
            << "\" max_distance=\"" << base->get_max_distance()
            << "\"></knowledge_base>" << std::endl;

        (*os)
            << "<params timeout_lhs=\"" << timeout_lhs()
            << "\" timeout_ilp=\"" << timeout_ilp()
            << "\" timeout_sol=\"" << timeout_sol()
            << "\" timeout_all=\"" << timeout_all()
            << "\" verbose=\"" << verbose();

        for (auto it = m_params.begin(); it != m_params.end(); ++it)
            (*os) << "\" " << it->first << "=\"" << it->second;

        for (auto it = m_flags.begin(); it != m_flags.end(); ++it)
            (*os) << "\" " << (*it) << "=\"yes";

#ifdef DISABLE_CANCELING
        (*os) << "\" disable_canceling=\"yes";
#endif

#ifdef DISABLE_HARD_TERM
        (*os) << "\" disable_hard_term=\"yes";
#endif

        (*os) << "\"></params>" << std::endl;

        (*os) << "</configure>" << std::endl;
    };

    auto f_write = [&](const std::string &key)
    {
        std::ofstream *fo(NULL);
        if ((fo = _open_file(param(key), (std::ios::out | std::ios::trunc))) != NULL)
        {
            write(fo);
            delete fo;
        }
    };

    f_write("path_lhs_out");
    f_write("path_ilp_out");
    f_write("path_sol_out");
    f_write("path_out");
    write(&std::cout);
}


void phillip_main_t::write_footer() const
{
    auto write = [this](std::ostream *os)
    {
        (*os) << "</phillip>" << std::endl;
    };
    auto f_write = [&](const std::string &key)
    {
        std::ofstream *fo(NULL);
        if ((fo = _open_file(param(key), (std::ios::out | std::ios::app))) != NULL)
        {
            write(fo);
            delete fo;
        }
    };

    f_write("path_lhs_out");
    f_write("path_ilp_out");
    f_write("path_sol_out");
    f_write("path_out");
    write(&std::cout);
}


}

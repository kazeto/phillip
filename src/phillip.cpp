/* -*- coding: utf-8 -*- */

#include <ctime>
#include <thread>
#include <algorithm>

#include "./phillip.h"
#include "./optimization.h"


namespace phil
{


namespace wf
{
const bits_t WR_FGEN = (1);
const bits_t WR_FCNV = (1 << 1);
const bits_t WR_FSOL = (1 << 2);
const bits_t WR_FOUT = (1 << 3);
const bits_t WR_ALL = (WR_FGEN | WR_FCNV | WR_FSOL | WR_FOUT);
}


int phillip_main_t::ms_verboseness = VERBOSE_1;
const std::string phillip_main_t::VERSION = "phil.3.17";


phillip_main_t::phillip_main_t()
: m_lhs_enumerator(NULL), m_ilp_convertor(NULL), m_ilp_solver(NULL),
  m_input(NULL), m_lhs(NULL), m_ilp(NULL),
  m_time_for_enumerate(0), m_time_for_convert(0), m_time_for_convert_gold(0),
  m_time_for_solve(0), m_time_for_solve_gold(0),
  m_time_for_learn(0), m_time_for_infer(0)
{}


phillip_main_t::~phillip_main_t()
{
    if (m_lhs_enumerator != NULL) delete m_lhs_enumerator;
    if (m_ilp_convertor != NULL)  delete m_ilp_convertor;
    if (m_ilp_solver != NULL)     delete m_ilp_solver;

    if (m_input != NULL) delete m_input;
    if (m_lhs != NULL)   delete m_lhs;
    if (m_ilp != NULL)   delete m_ilp;
}


std::ofstream* _open_file(const std::string &path, std::ios::openmode mode)
{
    if (not path.empty())
    {
        util::mkdir(util::get_directory_name(path));

        std::ofstream *fo = new std::ofstream(path.c_str(), mode);
        if (fo->good())
            return fo;
        else
        {
            util::print_error_fmt("Cannot open file: \"%s\"", path.c_str());
            delete fo;
        }
    }
    return NULL;
}


void phillip_main_t::infer(const lf::input_t &input)
{
    reset_for_inference();
    set_input(input);

    auto begin = std::chrono::system_clock::now();

    execute_enumerator();
    execute_convertor();
    execute_solver();

    m_time_for_infer = util::duration_time(begin);
}


void phillip_main_t::learn(const lf::input_t &input, opt::epoch_t epoch)
{
    auto get_path_for_gold = [this](const std::string &key) -> std::string
    {
        std::string path = param(key);
        if (not path.empty())
        {
            int idx = path.rfind('.');
            if (idx > 0)
                path = path.substr(0, idx) + ".gold" + path.substr(idx);
            else
                path += ".gold";
        }
        return path;
    };

    reset_for_inference();
    set_input(input);

    auto begin = std::chrono::system_clock::now();

    erase_flag("get_pseudo_positive");

    execute_enumerator();
    execute_convertor();
    execute_solver();

    if (not m_sol.front().is_positive_answer())
        return; // IF THE ANSWER WAS TRUE, TRAINING IS SKIPPED.

    set_flag("get_pseudo_positive");

    execute_convertor(
        &m_ilp_gold, &m_time_for_convert_gold,
        get_path_for_gold("path_ilp_out"));
    execute_solver(
        &m_sol_gold, &m_time_for_solve_gold,
        get_path_for_gold("path_sol_out"));

    std::unique_ptr<opt::training_result_t> result(
        m_ilp_convertor->train(epoch, m_sol.front(), m_sol_gold.front()));

    m_time_for_learn = util::duration_time(begin);
}


void phillip_main_t::execute_enumerator(
    pg::proof_graph_t **out_lhs, duration_time_t *out_time,
    const std::string &path_out_xml)
{
    IF_VERBOSE_2("Generating latent-hypotheses-set...");

    if ((*out_lhs) != NULL) delete m_lhs;

    auto begin = std::chrono::system_clock::now();
    (*out_lhs) = m_lhs_enumerator->execute();
    (*out_time) = util::duration_time(begin);

    IF_VERBOSE_2(
        m_lhs->has_timed_out() ?
        "Interrupted generating latent-hypotheses-set." :
        "Completed generating latent-hypotheses-set.");

    if (not path_out_xml.empty())
    {       
        std::ios::openmode mode = std::ios::out | std::ios::app;
        std::ofstream *fo = _open_file(path_out_xml, mode);
        if (fo != NULL)
        {
            m_lhs->print(fo);
            delete fo;
        }
    }
}


void phillip_main_t::execute_convertor(
    ilp::ilp_problem_t **out_ilp, duration_time_t *out_time,
    const std::string &path_out_xml)
{
    IF_VERBOSE_2("Converting LHS into linear-programming-problems...");

    auto begin = std::chrono::system_clock::now();
    (*out_ilp) = m_ilp_convertor->execute();
    (*out_time) = util::duration_time(begin);

    IF_VERBOSE_2(
        m_ilp->has_timed_out() ?
        "Interrupted convertion into linear-programming-problems." :
        "Completed convertion into linear-programming-problems.");

    if (not path_out_xml.empty())
    {
        std::ios::openmode mode = std::ios::out | std::ios::app;
        std::ofstream *fo = _open_file(path_out_xml, mode);
        if (fo != NULL)
        {
            m_ilp->print(fo);
            delete fo;
        }
    }
}


void phillip_main_t::execute_solver(
    std::vector<ilp::ilp_solution_t> *out_sols,
    duration_time_t *out_time,
    const std::string &path_out_xml)
{
    IF_VERBOSE_2("Solving...");

    auto begin = std::chrono::system_clock::now();
    m_ilp_solver->execute(out_sols);
    (*out_time) = util::duration_time(begin);

    IF_VERBOSE_2("Completed inference.");

    if (not path_out_xml.empty())
    {
        std::ios::openmode mode = std::ios::out | std::ios::app;
        std::ofstream *fo = _open_file(path_out_xml, mode);
        if (fo != NULL)
        {
            for (auto sol = m_sol.begin(); sol != m_sol.end(); ++sol)
                sol->print(fo);
            delete fo;
        }
    }
}


void phillip_main_t::write(const std::function<void(std::ostream*)> &writer, bits_t flags) const
{
    auto open_and_write = [&writer](const std::string &filename)
    {
        std::ofstream *fo(NULL);
        if ((fo = _open_file(filename, (std::ios::out | std::ios::trunc))) != NULL)
        {
            writer(fo);
            delete fo;
        }
    };

    if (flags & wf::WR_FGEN) open_and_write(param("path_lhs_out"));
    if (flags & wf::WR_FCNV) open_and_write(param("path_ilp_out"));
    if (flags & wf::WR_FSOL) open_and_write(param("path_sol_out"));
    if (flags & wf::WR_FOUT)
    {
        open_and_write(param("path_out"));
        writer(&std::cout);
    }
}


void phillip_main_t::write_header() const
{
    write([this](std::ostream *os){ write_header(os); });
}


void phillip_main_t::write_header(std::ostream *os) const
{
    (*os) << "<phillip>" << std::endl;
    (*os) << "<configure>" << std::endl;
    (*os) << "<version>" << VERSION << "</version>" << std::endl;

    auto get_time_stamp_exe = []() -> std::string
        {
            int year, month, day, hour, min, sec;
            std::string out;
            util::beginning_time(&year, &month, &day, &hour, &min, &sec);
            switch (month)
            {
            case 1:  out = "Jan"; break;
            case 2:  out = "Feb"; break;
            case 3:  out = "Mar"; break;
            case 4:  out = "Apr"; break;
            case 5:  out = "May"; break;
            case 6:  out = "Jun"; break;
            case 7:  out = "Jul"; break;
            case 8:  out = "Aug"; break;
            case 9:  out = "Sep"; break;
            case 10: out = "Oct"; break;
            case 11: out = "Nov"; break;
            case 12: out = "Dec"; break;
            default: throw;
            }
            return out + util::format(" %2d %4d %02d:%02d:%02d", day, year, hour, min, sec);
        };
        
    (*os)
        << "<time_stamp compiled=\"" << util::format("%s %s", __DATE__, __TIME__)
        << "\" executed=\"" << get_time_stamp_exe()
        << "\"></time_stamp>" << std::endl;

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
        << "<params timeout_lhs=\"" << timeout_lhs().get()
        << "\" timeout_ilp=\"" << timeout_ilp().get()
        << "\" timeout_sol=\"" << timeout_sol().get()
        << "\" timeout_all=\"" << timeout_all().get()
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
}


void phillip_main_t::write_footer() const
{
    write([this](std::ostream *os){ write_footer(os); });
}


void phillip_main_t::write_footer(std::ostream *os) const
{
    (*os) << "</phillip>" << std::endl;
}



}

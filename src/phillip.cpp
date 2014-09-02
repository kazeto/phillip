/* -*- coding: utf-8 -*- */

#include <ctime>
#include <thread>
#include <algorithm>

#include "./phillip.h"


namespace phil
{


int phillip_main_t::ms_verboseness = 0;


phillip_main_t::phillip_main_t()
: m_lhs_enumerator(NULL), m_ilp_convertor(NULL), m_ilp_solver(NULL),
  m_input(NULL), m_lhs(NULL), m_ilp(NULL),
  m_timeout_lhs(-1), m_timeout_ilp(-1), m_timeout_sol(-1),
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


void phillip_main_t::infer_parallel(
    const std::vector<lf::input_t> &inputs, size_t idx,
    bool do_print_on_each_thread)
{

    if (not can_infer())
    {
        print_error("Henry cannot infer!!");
        if (m_lhs_enumerator == NULL)
            print_error("    - No lhs_enumerator!");
        if (m_ilp_convertor == NULL)
            print_error("    - No ilp_convertor!");
        if (m_ilp_solver == NULL)
            print_error("    - No ilp_solver!");

        return;
    }

    bool is_begin(idx == 0), is_end(idx == inputs.size() - 1);
    std::ios::openmode mode =
        std::ios::out | (is_begin ? std::ios::trunc : std::ios::app);
    std::ofstream *fo(NULL);

    reset_for_inference();
    m_input = new lf::input_t(inputs.at(idx));
    clock_t begin_infer(clock());

    std::vector<lf::input_t> splitted = split_input(inputs.at(idx));
    for (int j = 0; j < splitted.size(); ++j)
    {
        phillip_main_t *ph = this->duplicate();
        const std::string &path_lhs(ph->param("path_lhs_out"));
        const std::string &path_ilp(ph->param("path_ilp_out"));
        const std::string &path_sol(ph->param("path_sol_out"));
        const std::string &path_out(ph->param("path_out"));
        if (not path_lhs.empty())
            ph->set_param("path_lhs_out",    
            do_print_on_each_thread ? indexize_path(path_lhs, j) : "");
        if (not path_ilp.empty())
            ph->set_param("path_ilp_out",
            do_print_on_each_thread ? indexize_path(path_ilp, j) : "");
        if (not path_sol.empty())
            ph->set_param("path_sol_out",
            do_print_on_each_thread ? indexize_path(path_sol, j) : "");
        if (not path_out.empty())
            ph->set_param("path_out",
            do_print_on_each_thread ? indexize_path(path_out, j) : "");
        m_phillips_parallel.push_back(ph);
    }
    IF_VERBOSE_1(
        format("# of parallel processes = %d", m_phillips_parallel.size()));

    std::vector<std::thread> worker;
    int num_thread = std::min<int>(
        splitted.size(),
        std::max<int>(std::thread::hardware_concurrency(), 1));
    for (int i = 0; i < num_thread; ++i)
    {
        worker.emplace_back([&](int id){
            int r0 = splitted.size() * id / num_thread +
                std::min<int>(splitted.size() % num_thread, id);
            int r1 = splitted.size() * (id + 1) / num_thread +
                std::min<int>(splitted.size() % num_thread, id + 1);
            for (int j = r0; j < r1; ++j)
            {
                phillip_main_t *ph = m_phillips_parallel.at(j);
                lf::input_t ipt = splitted.at(j);
                ph->infer(ipt);
            }
        }, i);
    }
    for (auto &t : worker) t.join();

    m_lhs = new pg::proof_graph_t(this, m_input->name);
    m_ilp = new ilp::ilp_problem_t(
        m_lhs, new ilp::basic_solution_interpreter_t(), true, m_input->name);
    m_sol.push_back(ilp::ilp_solution_t(
        m_ilp, ilp::SOLUTION_OPTIMAL, std::vector<double>(), m_input->name));

    m_clock_for_infer = clock() - begin_infer;

    for (auto it = m_phillips_parallel.begin();
        it != m_phillips_parallel.end(); ++it)
    {
        m_lhs->merge(*(*it)->get_latent_hypotheses_set());
        m_ilp->merge(*(*it)->get_ilp_problem());
        m_sol[0].merge((*it)->get_solutions().at(0));
        m_clock_for_enumerate =
            std::max<int>(m_clock_for_enumerate, (*it)->get_clock_for_lhs());
        m_clock_for_convert =
            std::max<int>(m_clock_for_convert, (*it)->get_clock_for_ilp());
        m_clock_for_solve =
            std::max<int>(m_clock_for_solve, (*it)->get_clock_for_sol());
    }

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


std::vector<lf::input_t>
phillip_main_t::split_input(const lf::input_t &input) const
{
    std::vector<lf::input_t> splitted;

    auto is_clustered = [](const literal_t &lit, const lf::input_t &ipt) -> bool
    {
        auto base = kb::knowledge_base_t::instance();
        auto obs = ipt.obs.get_all_literals();
        auto req = ipt.req.get_all_literals();
        std::list<const literal_t*> lits;

        lits.insert(lits.end(), obs.begin(), obs.end());
        lits.insert(lits.end(), req.begin(), req.end());

        for (auto it = lits.begin(); it != lits.end(); ++it)
        {
            float dist = base->get_distance(
                (*it)->get_predicate_arity(), lit.get_predicate_arity());
            if (dist >= 0) return true;

            // TODO: SPLITTING ON TERM
        }

        return false;
    };

    auto merge = [](const lf::input_t &src, lf::input_t *dest)
    {
        auto obs = src.obs.branches();
        for (auto it = obs.begin(); it != obs.end(); ++it)
            dest->obs.add_branch(*it);

        auto req = src.req.branches();
        for (auto it = req.begin(); it != req.end(); ++it)
            dest->req.add_branch(*it);
    };

    auto journalize = [&](
        const std::vector<lf::logical_function_t> &branches,
        bool is_observation)
    {
        for (auto it = branches.begin(); it != branches.end(); ++it)
        {
            auto lits = it->get_all_literals();
            hash_set<int> idx;
            for (int i = 0; i < splitted.size(); ++i)
            for (auto it2 = lits.begin(); it2 != lits.end(); ++it2)
            {
                if (is_clustered((**it2), splitted.at(i)))
                    idx.insert(i);
            }

            if (idx.empty())
            {
                lf::input_t _new;
                _new.name = input.name;
                if (is_observation)
                {
                    _new.obs = lf::logical_function_t(lf::OPR_AND);
                    _new.obs.add_branch(*it);
                }
                else
                {
                    _new.req = lf::logical_function_t(lf::OPR_REQUIREMENT);
                    _new.req.add_branch(*it);
                }
                splitted.push_back(_new);
            }
            else
            {
                std::list<int> _idx(idx.begin(), idx.end());
                _idx.sort();

                int i_dest = _idx.front();
                _idx.pop_front();

                if (not idx.empty())
                for (auto i = _idx.rbegin(); i != _idx.rend(); ++i)
                if (*i != i_dest)
                {
                    merge(splitted[*i], &splitted[i_dest]);
                    erase(splitted, *i);
                }

                lf::input_t &ipt(splitted[i_dest]);
                (is_observation ? ipt.obs : ipt.req).add_branch(*it);
            }
        }
    };

    journalize(input.obs.branches(), true);
    journalize(input.req.branches(), false);

    for (int i = 0; i < splitted.size(); ++i)
        splitted[i].name += format("-%d", i);

    return splitted;
}


void phillip_main_t::write_configure(std::ofstream *fo) const
{
    (*fo) << "<configure>" << std::endl;

    (*fo) << "<components lhs=\"" << m_lhs_enumerator->repr()
          << "\" ilp=\"" << m_ilp_convertor->repr()
          << "\" sol=\"" << m_ilp_solver->repr()
          << "\"></components>" << std::endl;
    
    (*fo) << "<params timeout_lhs=\"" << timeout_lhs()
          << "\" timeout_ilp=\"" << timeout_ilp()
          << "\" timeout_sol=\"" << timeout_sol()
          << "\" verbose=\"" << verbose();
    
    for (auto it = m_params.begin(); it != m_params.end(); ++it)
        (*fo) << "\" " << it->first << "=\"" << it->second;
    
    for (auto it = m_flags.begin(); it != m_flags.end(); ++it)
        (*fo) << "\" " << (*it) << "=\"yes";
            
    (*fo) << "\"></params>" << std::endl;
    
    (*fo) << "</configure>" << std::endl;
}


}

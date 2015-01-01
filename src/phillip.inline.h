#ifndef INCLUDE_HENRY_HENRY_INLINE_H
#define INCLUDE_HENRY_HENRY_INLINE_H


namespace phil
{


inline void phillip_main_t::set_verbose(int v)
{
    ms_verboseness = v;
}


inline const int phillip_main_t::verbose()
{
    return ms_verboseness;
}


inline void phillip_main_t::infer(const lf::input_t &input)
{
    std::vector<lf::input_t> inputs(1, input);
    infer(inputs, 0);
}


inline const lhs_enumerator_t* phillip_main_t::lhs_enumerator() const
{
    return m_lhs_enumerator;
}


inline lhs_enumerator_t* phillip_main_t::lhs_enumerator()
{
    return m_lhs_enumerator;
}


inline const ilp_converter_t* phillip_main_t::ilp_convertor() const
{
    return m_ilp_convertor;
}


inline ilp_converter_t* phillip_main_t::ilp_convertor()
{
    return m_ilp_convertor;
}


inline const ilp_solver_t* phillip_main_t::ilp_solver() const
{
    return m_ilp_solver;
}


inline ilp_solver_t* phillip_main_t::ilp_solver()
{
    return m_ilp_solver;
}


inline void phillip_main_t::set_lhs_enumerator(lhs_enumerator_t *ptr)
{
    if (m_lhs_enumerator != NULL)
        delete m_lhs_enumerator;
    m_lhs_enumerator = ptr;
}


inline void phillip_main_t::set_ilp_convertor(ilp_converter_t *ptr)
{
    if (m_ilp_convertor != NULL)
        delete m_ilp_convertor;
    m_ilp_convertor = ptr;
}


inline void phillip_main_t::set_ilp_solver(ilp_solver_t *ptr)
{
    if (m_ilp_solver != NULL)
        delete m_ilp_solver;
    m_ilp_solver = ptr;
}


inline void phillip_main_t::set_timeout_lhs(int t)
{ m_timeout_lhs = t; }


inline void phillip_main_t::set_timeout_ilp(int t)
{ m_timeout_ilp = t; }


inline void phillip_main_t::set_timeout_sol(int t)
{ m_timeout_sol = t; }


inline void phillip_main_t::set_param(
    const std::string &key, const std::string &param )
{ m_params[key] = param; }


inline void phillip_main_t::set_flag(const std::string &key)
{ m_flags.insert(key); }


inline const lf::input_t* phillip_main_t::get_input() const
{ return m_input; }


inline const lf::logical_function_t* phillip_main_t::get_observation() const
{ return (m_input != NULL) ? &m_input->obs : NULL; }


inline const lf::logical_function_t* phillip_main_t::get_requirement() const
{ return (m_input != NULL) ? &m_input->req : NULL; }


inline const pg::proof_graph_t* phillip_main_t::get_latent_hypotheses_set() const
{ return m_lhs; }


inline const ilp::ilp_problem_t* phillip_main_t::get_ilp_problem() const
{ return m_ilp; }


inline const std::vector<ilp::ilp_solution_t>& phillip_main_t::get_solutions() const
{ return m_sol; }


inline const std::vector<phillip_main_t*>& phillip_main_t::get_parallel_phillips() const
{ return m_phillips_parallel; }


inline int phillip_main_t::timeout_lhs() const
{ return m_timeout_lhs; }


inline int phillip_main_t::timeout_ilp() const
{ return m_timeout_ilp; }


inline int phillip_main_t::timeout_sol() const
{ return m_timeout_sol; }


inline bool phillip_main_t::is_timeout_lhs(int sec) const
{ return (m_timeout_lhs > 0 and sec >= m_timeout_lhs); }


inline bool phillip_main_t::is_timeout_ilp(int sec) const
{ return (m_timeout_ilp > 0 and sec >= m_timeout_ilp); }


inline bool phillip_main_t::is_timeout_sol(int sec) const
{ return (m_timeout_sol > 0 and sec >= m_timeout_sol); }


inline const hash_map<std::string, std::string>& phillip_main_t::params() const
{ return m_params; }


inline const std::string& phillip_main_t::param( const std::string &key ) const
{
    static const std::string empty_str("");
    return has_key(m_params, key) ? m_params.at(key) : empty_str;
}


inline int phillip_main_t::param_int(const std::string &key, int def) const
{
    int out(def);
    _sscanf(param(key).c_str(), "%d", &out);
    return out;
}


inline float phillip_main_t::param_float(const std::string &key, float def) const
{
    float out(def);
    _sscanf(param(key).c_str(), "%f", &out);
    return out;
}


inline const hash_set<std::string>& phillip_main_t::flags() const
{ return m_flags; }


inline bool phillip_main_t::flag(const std::string &key) const
{ return m_flags.find(key) != m_flags.end(); }


inline const long& phillip_main_t::get_clock_for_lhs() const
{ return m_clock_for_enumerate; }


inline const long& phillip_main_t::get_clock_for_ilp() const
{ return m_clock_for_convert; }


inline const long& phillip_main_t::get_clock_for_sol() const
{ return m_clock_for_solve; }


inline const long& phillip_main_t::get_clock_for_infer() const
{ return m_clock_for_infer; }


inline float phillip_main_t::get_time_for_lhs()  const
{ return (float)m_clock_for_enumerate / (float)CLOCKS_PER_SEC; }


inline float phillip_main_t::get_time_for_ilp()  const
{
    return (float)m_clock_for_convert / (float)CLOCKS_PER_SEC;
}


inline float phillip_main_t::get_time_for_sol()  const
{
    return (float)m_clock_for_solve / (float)CLOCKS_PER_SEC;
}


inline float phillip_main_t::get_time_for_infer() const
{
    return (float)m_clock_for_infer / (float)CLOCKS_PER_SEC;
}


inline void phillip_main_t::add_target(const std::string &name)
{
    m_target_obs_names.insert(name);
}


inline bool phillip_main_t::is_target(const std::string &name) const
{
    return m_target_obs_names.empty() ? true : (m_target_obs_names.count(name) > 0);
}


inline void phillip_main_t::add_exclusion(const std::string &name)
{
    m_excluded_obs_names.insert(name);
}


inline bool phillip_main_t::is_excluded(const std::string &name) const
{
    return m_excluded_obs_names.empty() ? false : (m_excluded_obs_names.count(name) > 0);
}


inline bool phillip_main_t::check_validity() const
{
    bool can_infer = 
        (m_lhs_enumerator != NULL) and
        (m_ilp_convertor != NULL) and
        (m_ilp_solver != NULL);

    if (not can_infer)
    {
        print_error("Henry cannot infer!!");
        if (lhs_enumerator() == NULL)
            print_error("    - No lhs_enumerator!");
        if (ilp_convertor() == NULL)
            print_error("    - No ilp_convertor!");
        if (ilp_solver() == NULL)
            print_error("    - No ilp_solver!");
    }

    return can_infer;
}


inline void phillip_main_t::reset_for_inference()
{
    if (m_input != NULL) delete m_input;
    if (m_lhs != NULL)   delete m_lhs;
    if (m_ilp != NULL)   delete m_ilp;

    m_input = NULL;
    m_lhs = NULL;
    m_ilp = NULL;

    m_clock_for_enumerate = 0;
    m_clock_for_convert = 0;
    m_clock_for_solve = 0;
    m_clock_for_infer = 0;

    m_sol.clear();

    for (auto it = m_phillips_parallel.begin();
        it != m_phillips_parallel.end(); ++it)
        delete (*it);
    m_phillips_parallel.clear();
}


}

#endif

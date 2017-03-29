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


inline void phillip_main_t::set_input(const lf::input_t &ipt)
{
    m_input.reset(new lf::input_t(ipt));
}


inline void phillip_main_t::set_param(
    const string_t &key, const string_t &param )
{
    m_params[key.replace("_", "-")] = param;
}


inline void phillip_main_t::erase_param(const std::string &key)
{
    m_params.erase(key);
}


inline void phillip_main_t::set_flag(const string_t &key)
{
    m_flags.insert(key.replace("_", "-"));
}


inline void phillip_main_t::erase_flag(const std::string &key)
{
    m_flags.erase(key);
}


inline const lf::input_t* phillip_main_t::get_input() const
{
    return m_input.get();
}


inline const lf::logical_function_t* phillip_main_t::get_observation() const
{
    return (m_input != NULL) ? &m_input->obs : NULL;
}


inline const lf::logical_function_t* phillip_main_t::get_requirement() const
{
    return (m_input != NULL) ? &m_input->req : NULL;
}


inline const pg::proof_graph_t* phillip_main_t::get_latent_hypotheses_set() const
{
    return m_lhs.get();
}


inline const ilp::ilp_problem_t* phillip_main_t::get_ilp_problem() const
{
    return m_ilp.get();
}


inline const std::vector<ilp::ilp_solution_t>& phillip_main_t::get_solutions() const
{
    return m_sol;
}


inline const std::vector<ilp::ilp_solution_t>& phillip_main_t::get_positive_answer() const
{
    return m_sol_gold;
}


inline const opt::training_result_t* phillip_main_t::get_training_result() const
{
    return m_train_result.get();
}


inline const hash_map<string_t, string_t>& phillip_main_t::params() const
{ return m_params; }


inline const string_t& phillip_main_t::param(const string_t &key) const
{
    static const string_t empty_str("");
    auto found = m_params.find(key);
    return (found != m_params.end()) ? found->second : empty_str;
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


inline const hash_set<string_t>& phillip_main_t::flags() const
{
    return m_flags;
}


inline bool phillip_main_t::flag(const std::string &key) const
{
    return m_flags.find(key) != m_flags.end();
}


inline bool phillip_main_t::do_infer_pseudo_positive() const
{
    return flag("get-pseudo-positive");
}


inline float phillip_main_t::get_time_for_lhs()  const
{
    return m_time_for_enumerate;
}


inline float phillip_main_t::get_time_for_ilp()  const
{
    return m_time_for_convert;
}


inline float phillip_main_t::get_time_for_sol()  const
{
    return m_time_for_solve;
}


inline float phillip_main_t::get_time_for_infer() const
{
    return m_time_for_infer;
}


inline void phillip_main_t::add_target(const std::string &name)
{
    m_target_obs_names.insert(name);
}


inline void phillip_main_t::clear_targets()
{
    m_target_obs_names.clear();
}


inline bool phillip_main_t::is_target(const std::string &name) const
{
    return m_target_obs_names.empty() ? true : (m_target_obs_names.count(name) > 0);
}


inline void phillip_main_t::add_exclusion(const std::string &name)
{
    m_excluded_obs_names.insert(name);
}


inline void phillip_main_t::clear_exclusions()
{
    m_excluded_obs_names.clear();
}


inline bool phillip_main_t::is_excluded(const std::string &name) const
{
    return m_excluded_obs_names.empty() ? false : (m_excluded_obs_names.count(name) > 0);
}


inline void phillip_main_t::reset_for_inference()
{
    m_input.reset();
    m_lhs.reset();
    m_ilp.reset();
    m_ilp_gold.reset();
    m_train_result.reset();

    m_time_for_enumerate = 0.0f;
    m_time_for_convert = 0.0f;
    m_time_for_convert_gold = 0.0f;
    m_time_for_solve = 0.0f;
    m_time_for_solve_gold = 0.0f;
    m_time_for_learn = 0.0f;
    m_time_for_infer = 0.0f;

    m_sol.clear();
    m_sol_gold.clear();
}


inline void phillip_main_t::execute_enumerator()
{
    execute_enumerator(
        &m_lhs, &m_time_for_enumerate, param("path-lhs-out"));
}


inline void phillip_main_t::execute_convertor()
{
    execute_convertor(
        &m_ilp, &m_time_for_convert, param("path-ilp-out"));
}


inline void phillip_main_t::execute_solver()
{
    execute_solver(
        m_ilp.get(), &m_sol, &m_time_for_solve, param("path-sol-out"));
}


}

#endif

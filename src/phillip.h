/* -*- coding: utf-8 -*- */

#pragma once

#include <string>
#include <map>

#include "./kb.h"
#include "./interface.h"
#include "./logical_function.h"
#include "./proof_graph.h"
#include "./ilp_problem.h"


/** A namespace of Henry.  */
namespace phil
{

class lhs_enumerator_t;
class ilp_converter_t;
class ilp_solver_t;


/** Main class of Phillip.
 *  The procedure of inference is following:
 *   - Create an instance of phillip_main_t.
 *   - Create components.
 *   - Set the components to phillip_main_t instance.
 *   - Call infer() with input observation. */
class phillip_main_t
{
public:
    static inline void set_verbose(int v);
    static inline const int verbose();

    static const std::string VERSION;

    phillip_main_t();
    ~phillip_main_t();

    phillip_main_t* duplicate() const;
    
    /** Infer a explanation to given observation.
     *  You can get the results via accesser functions.
     *  @param inputs A list of observations.
     *  @param idx    Index of an observation to infer. */
    void infer(const std::vector<lf::input_t> &inputs, size_t idx);
    inline void infer(const lf::input_t &input);

    void infer_parallel(
        const std::vector<lf::input_t> &inputs, size_t idx,
        bool do_print_on_each_thread = false);
    
    inline const lhs_enumerator_t* lhs_enumerator() const;
    inline lhs_enumerator_t* lhs_enumerator();
    inline const ilp_converter_t* ilp_convertor() const;
    inline ilp_converter_t* ilp_convertor();
    inline const ilp_solver_t* ilp_solver() const;
    inline ilp_solver_t* ilp_solver();

    inline void set_lhs_enumerator(lhs_enumerator_t*);
    inline void set_ilp_convertor(ilp_converter_t*);
    inline void set_ilp_solver(ilp_solver_t*);

    inline void set_timeout_lhs(int t);
    inline void set_timeout_ilp(int t);
    inline void set_timeout_sol(int t);
    inline void set_param(const std::string &key, const std::string &param);
    inline void set_flag(const std::string &key);
    
    inline const lf::input_t* get_input() const;
    inline const lf::logical_function_t* get_observation() const;
    inline const lf::logical_function_t* get_requirement() const;
    inline const pg::proof_graph_t* get_latent_hypotheses_set() const;
    inline const ilp::ilp_problem_t* get_ilp_problem() const;
    inline const std::vector<ilp::ilp_solution_t>& get_solutions() const;
    inline const std::vector<phillip_main_t*>& get_parallel_phillips() const;
    
    inline int timeout_lhs() const;
    inline int timeout_ilp() const;
    inline int timeout_sol() const;
    inline bool is_timeout_lhs(int sec) const;
    inline bool is_timeout_ilp(int sec) const;
    inline bool is_timeout_sol(int sec) const;

    inline const hash_map<std::string, std::string>& params() const;
    inline const std::string& param(const std::string &key) const;
    inline int param_int(const std::string &key, int def = -1) const;
    inline float param_float(const std::string &key, float def = -1.0f) const;

    inline const hash_set<std::string>& flags() const;
    inline bool flag(const std::string &key) const;

    inline const long& get_clock_for_lhs()  const;
    inline const long& get_clock_for_ilp()  const;
    inline const long& get_clock_for_sol()  const;
    inline const long& get_clock_for_infer() const;

    inline float get_time_for_lhs()  const;
    inline float get_time_for_ilp()  const;
    inline float get_time_for_sol()  const;
    inline float get_time_for_infer() const;

    inline void add_target(const std::string &name);
    inline bool is_target(const std::string &name) const;
    inline void add_exclusion(const std::string &name);
    inline bool is_excluded(const std::string &name) const;
    
    void write_configure(std::ostream *fo) const;

private:
    enum process_mode_e { MODE_COMPILE_KB, MODE_INPUT_OBS };
    
    bool interpret_option(int opt, const char *optarg);
    
    inline bool can_infer() const;
    inline void reset_for_inference();

    std::vector<lf::input_t> split_input(const lf::input_t&) const;

    static int ms_verboseness;

    // ---- FUNCTION CLASS OF EACH PROCEDURE
    lhs_enumerator_t *m_lhs_enumerator;
    ilp_converter_t  *m_ilp_convertor;
    ilp_solver_t     *m_ilp_solver;

    // ---- DATA, SETTING, ETC...
    hash_map<std::string, std::string> m_params;
    hash_set<std::string> m_flags;
    int  m_timeout_lhs, m_timeout_ilp, m_timeout_sol;
    
    hash_set<std::string> m_target_obs_names;
    hash_set<std::string> m_excluded_obs_names;

    // ---- PRODUCTS OF INFERENCE
    lf::input_t *m_input;
    pg::proof_graph_t *m_lhs;
    ilp::ilp_problem_t *m_ilp;
    std::vector<ilp::ilp_solution_t> m_sol;

    // ---- FOR MEASURE TIME
    long m_clock_for_enumerate;
    long m_clock_for_convert;
    long m_clock_for_solve;
    long m_clock_for_infer;

    std::vector<phillip_main_t*> m_phillips_parallel;
};


}


#define IF_VERBOSE(V, E) if(phillip_main_t::verbose() >= V) print_console(E);

#define IF_VERBOSE_1(E) IF_VERBOSE(phil::VERBOSE_1, E)
#define IF_VERBOSE_2(E) IF_VERBOSE(phil::VERBOSE_2, E)
#define IF_VERBOSE_3(E) IF_VERBOSE(phil::VERBOSE_3, E)
#define IF_VERBOSE_4(E) IF_VERBOSE(phil::VERBOSE_4, E)
#define IF_VERBOSE_FULL(E) IF_VERBOSE(phil::FULL_VERBOSE, E)


#include "./phillip.inline.h"


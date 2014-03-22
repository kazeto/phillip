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
 *  This class is singleton class.
 *  The procedure of inference is following:
 *   - Create an instance of phillip_main_t.
 *   - Create components.
 *   - Set the components to phillip_main_t instance.
 *   - Call infer() with input observation. */
class phillip_main_t
{
public:
    static inline phillip_main_t* get_instance();
    
    /** Infer a explanation to given observation.
     *  You can get the results via accesser functions.
     *  @param do_append Identifies the option of file open, app or trunc. */
    void infer(const lf::input_t &input, bool do_append = false);
    
    inline const lhs_enumerator_t* lhs_enumerator() const; 
    inline const ilp_converter_t*  ilp_convertor() const;
    inline const ilp_solver_t*     ilp_solver() const;
    inline const kb::knowledge_base_t *knowledge_base() const;

    inline void set_lhs_enumerator(const lhs_enumerator_t*);
    inline void set_ilp_convertor(const ilp_converter_t*);
    inline void set_ilp_solver(const ilp_solver_t*);
    inline void set_knowledge_base(kb::knowledge_base_t *kb);

    inline void set_timeout(int t);
    inline void set_verbose(int v);
    inline void set_debug_flag(bool flag);
    inline void set_param(const std::string &key, const std::string &param);
    inline void set_flag(const std::string &key);
    
    inline const lf::input_t* get_input() const;
    inline const lf::logical_function_t* get_observation() const;
    inline const pg::proof_graph_t* get_latent_hypotheses_set() const;
    inline const ilp::ilp_problem_t* get_ilp_problem() const;
    inline const std::vector<ilp::ilp_solution_t>& get_solutions() const;
    
    inline const int& timeout() const;
    inline const int& verbose() const;
    inline bool is_debugging()  const;

    inline const hash_map<std::string, std::string>& params() const;
    inline const std::string& param( const std::string &key ) const;
    inline const hash_set<std::string>& flags() const;
    inline bool flag(const std::string &key) const;

    inline const long& get_clock_for_flhs()  const;
    inline const long& get_clock_for_flpp()  const;
    inline const long& get_clock_for_fsol()  const;
    inline const long& get_clock_for_infer() const;
    
private:
    enum process_mode_e { MODE_COMPILE_KB, MODE_INPUT_OBS };
    
    inline phillip_main_t();

    bool interpret_option( int opt, const char *optarg );
    
    inline bool can_infer() const;
    inline void reset_for_inference();

    static phillip_main_t *ms_instance;
    
    // ---- FUNCTION CLASS OF EACH PROCEDURE
    const lhs_enumerator_t *m_lhs_enumerator;
    const ilp_converter_t  *m_ilp_convertor;
    const ilp_solver_t     *m_ilp_solver;

    kb::knowledge_base_t *m_kb;
    
    // ---- DATA, SETTING, ETC...
    hash_map<std::string, std::string> m_params;
    hash_set<std::string> m_flags;
    int  m_timeout;
    int  m_verboseness;
    bool m_is_debugging;

    // ---- PRODUCTS OF INFERENCE
    lf::input_t *m_input;
    pg::proof_graph_t *m_lhs;
    ilp::ilp_problem_t *m_ilp;
    std::vector<ilp::ilp_solution_t> m_sol;

    // ---- FOR MEASURE TIME
    long m_clock_for_enumeration;
    long m_clock_for_convention;
    long m_clock_for_solution;
    long m_clock_for_infer;
};


inline phillip_main_t *sys() { return phillip_main_t::get_instance(); }

}


#define IF_VERBOSE(V, E) if(phil::sys()->verbose() >= V) print_console(E);

#define IF_VERBOSE_1(E) IF_VERBOSE(phil::VERBOSE_1, E)
#define IF_VERBOSE_2(E) IF_VERBOSE(phil::VERBOSE_2, E)
#define IF_VERBOSE_3(E) IF_VERBOSE(phil::VERBOSE_3, E)
#define IF_VERBOSE_4(E) IF_VERBOSE(phil::VERBOSE_4, E)
#define IF_VERBOSE_FULL(E) IF_VERBOSE(phil::FULL_VERBOSE, E)


#include "./phillip.inline.h"


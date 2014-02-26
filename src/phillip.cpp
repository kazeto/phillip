/* -*- coding: utf-8 -*- */


#include <time.h>
#include "./phillip.h"


namespace phil
{


phillip_main_t *phillip_main_t::ms_instance = NULL;


void phillip_main_t::infer( const lf::input_t &input )
{
    if( not can_infer() )
    {
        print_error( "Henry cannot infer!!" );
        if( m_lhs_enumerator == NULL )
            print_error( "    - No lhs_enumerator!" );
        if( m_ilp_convertor == NULL )
            print_error( "    - No ilp_convertor!" );
        if( m_ilp_solver == NULL )
            print_error( "    - No ilp_solver!" );
        if( m_kb == NULL )
            print_error( "    - No knowledge_base!" );

        return;
    }
    
    reset_for_inference();
    m_obs = new lf::logical_function_t( input.obs );

    clock_t begin_infer( clock() );
    
    IF_VERBOSE_2( "Generating latent-hypotheses-set..." );
    clock_t begin_flhs( clock() );
    m_lhs = m_lhs_enumerator->execute();
    clock_t end_flhs( clock() );
    m_clock_for_enumeration += end_flhs - begin_flhs;
    IF_VERBOSE_2( "Completed generating latent-hypotheses-set." );

    if (not param("path_lhs_out").empty())
    {
        std::ofstream fo(param("path_lhs_out").c_str());
        m_lhs->print(&fo);
        fo.close();
    }

    IF_VERBOSE_2( "Converting LHS into linear-programming-problems..." );
    clock_t begin_flpp( clock() );
    m_ilp = m_ilp_convertor->execute();
    clock_t end_flpp( clock() );
    m_clock_for_convention += end_flpp - begin_flpp;
    IF_VERBOSE_2( "Completed convertion into linear-programming-problems..." );

    if (not param("path_ilp_out").empty())
    {
        std::ofstream fo(param("path_ilp_out").c_str());
        m_ilp->print(&fo);
        fo.close();
    }

    IF_VERBOSE_2( "Solving..." );
    clock_t begin_fsol( clock() );
    m_ilp_solver->execute(&m_sol);
    clock_t end_fsol( clock() );
    m_clock_for_solution += end_fsol - begin_fsol;
    IF_VERBOSE_2( "Completed inference." );

    for (auto sol = m_sol.begin(); sol != m_sol.end(); ++sol)
        sol->print_graph();

    if (not param("path_sol_out").empty())
    {
        std::ofstream fo(param("path_sol_out").c_str());
        for (auto sol = m_sol.begin(); sol != m_sol.end(); ++sol)
            sol->print(&fo);
        fo.close();
    }

    if (not param("path_out").empty())
    {
        std::ofstream fo(param("path_out").c_str());
        for (auto sol = m_sol.begin(); sol != m_sol.end(); ++sol)
            sol->print_graph(&fo);
        fo.close();
    }

    clock_t end_infer( clock() );
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

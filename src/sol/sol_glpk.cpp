/* -*- coding:utf-8 -*- */

#include "./ilp_solver.h"


namespace phil
{

namespace sol
{


gnu_linear_programming_kit_t::gnu_linear_programming_kit_t()
{}


void gnu_linear_programming_kit_t::execute(
    std::vector<ilp::ilp_solution_t> *out ) const
{
#ifdef USE_GLPK
    const ilp::ilp_problem_t *lpp =
        sys()->get_ilp_problem();

    glp_prob *ilp = glp_create_prob();
    setup( ilp );

    glp_simplex( ilp, NULL );
#endif
}


bool gnu_linear_programming_kit_t::can_execute(
    std::list<std::string> *error_messages ) const
{
#ifdef USE_GLPK
    return true;
#else
    return false;
#endif
}


std::string gnu_linear_programming_kit_t::repr() const
{
    return "GNU-Linear-Programming-Kit";
}


#ifdef USE_GLPK
void gnu_linear_programming_kit_t::setup( glp_prob *ilp ) const
{
    const ilp::ilp_problem_t *lpp =
        sys()->get_ilp_problem();
    std::vector<int> rows, cols;
    std::vector<double> coef;

    glp_set_prob_name(ilp, "Henry");
    glp_set_obj_dir(ilp, GLP_MAX);

    glp_add_rows( ilp, lpp->constraints().size() );
    glp_add_cols( ilp, lpp->variables().size() );
    rows.push_back(NULL);
    cols.push_back(NULL);
    coef.push_back(NULL);

    for( int i=0; i<lpp->variables().size(); ++i )
    {
        const ilp::variable_t &var = lpp->variable(i);
        int idx(i + 1);
        glp_set_col_name( ilp, idx, var.name().c_str() );
        glp_set_obj_coef( ilp, idx, var.objective_coefficient() );
    }

    for( int i=0; i<lpp->constraints().size(); ++i )
    {
        const ilp::constraint_t &cons = lpp->constraint(i);
        int glp_type(-1), idx(i + 1);

        switch( cons.operator_type() )
        {
        case ilp::OPR_EQUAL:      glp_type = GLP_FX; break;
        case ilp::OPR_LESS_EQ:    glp_type = GLP_UP; break;
        case ilp::OPR_GREATER_EQ: glp_type = GLP_LO; break;
        case ilp::OPR_RANGE:      glp_type = GLP_DB; break;
        }
        if( glp_type < 0 ) continue;

        glp_set_row_name( ilp, idx, cons.name().c_str() );
        glp_set_row_bnds(
            ilp, idx, glp_type, cons.lower_bound(), cons.upper_bound() );

        for( auto it=cons.terms().begin(); it!=cons.terms().end(); ++it )
        {
            rows.push_back( idx );
            cols.push_back( it->var_idx + 1 );
            coef.push_back( it->coefficient );
        }
    }

    glp_load_matrix( ilp, rows.size() - 1, &rows[0], &cols[0], &coef[0] );
}
#endif



}

}

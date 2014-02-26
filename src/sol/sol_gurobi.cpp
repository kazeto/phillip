#include "./ilp_solver.h"

namespace phil
{

namespace sol
{


void gurobi_t::execute(std::vector<ilp::ilp_solution_t> *out) const
{
#ifdef USE_GUROBI
    const ilp::ilp_problem_t *prob = sys()->get_ilp_problem();
    GRBEnv env;
    GRBModel model(env);
    hash_map<ilp::variable_idx_t, GRBVar> vars;

    add_variables(&model, &vars);
    add_constraints(&model, &vars);

    model.set(GRB_IntAttr_ModelSense, GRB_MAXIMIZE);
    model.optimize();
#endif
}


bool gurobi_t::can_execute(std::list<std::string> *err) const
{
#ifdef USE_GUROBI
    return true;
#else
    return false;
#endif
}


std::string gurobi_t::repr() const
{
    return "Gurobi-Optimizer";
}


#ifdef USE_GUROBI

void gurobi_t::add_variables(
    GRBModel *model, hash_map<ilp::variable_idx_t, GRBVar> *vars) const
{
    const ilp::ilp_problem_t *prob = sys()->get_ilp_problem();

    for (int i = 0; i < prob->variables().size(); ++i)
    {
        const ilp::variable_t &v = prob->variable(i);
        (*vars)[i] =
            model->addVar(0.0, 1.0, v.objective_coefficient(), GRB_BINARY);
        if (prob->is_constant_variable(i))
            (*vars)[i].set(GRB_DoubleAttr_Start, prob->const_variable_value(i));
    }
}


void gurobi_t::add_constraints(
    GRBModel *model, hash_map<ilp::variable_idx_t, GRBVar> *vars) const
{
    const ilp::ilp_problem_t *prob = sys()->get_ilp_problem();

    for (int i = 0; i < prob->constraints().size(); ++i)
    {
        const ilp::constraint_t &c = prob->constraint(i);
        GRBLinExpr expr;

        for (auto t = c.terms().begin(); t != c.terms().end(); ++t)
            expr += t->coefficient * vars->at(t->var_idx);

        switch (c.operator_type())
        {
        case ilp::OPR_EQUAL:
            model->addConstr(
                expr, GRB_EQUAL, c.bound(), c.name().substr(0, 32));
            break;
        case ilp::OPR_LESS_EQ:
            model->addConstr(
                expr, GRB_LESS_EQUAL, c.upper_bound(),
                c.name().substr(0, 32));
            break;
        case ilp::OPR_GREATER_EQ:
            model->addConstr(
                expr, GRB_GREATER_EQUAL, c.lower_bound(),
                c.name().substr(0, 32));
            break;
        case ilp::OPR_RANGE:
            model->addConstr(
                expr, c.lower_bound(), c.upper_bound(),
                c.name().substr(0, 32));
            break;
        }
    }
}


#endif

}

}
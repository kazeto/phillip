#include <phillip.h>
#include <binary.h>


/** A example class of user-defined ilp-converter.
 *  The evaluation function expressed in this converter is as follows:
 *  E(H) = w_u * {# of unification in H} - w_b * {# of backchaining in H}
 *  where w_u and w_b are customizable weights. */
class my_ilp_converter_t : public phil::ilp_converter_t
{
public:
    my_ilp_converter_t(phil::phillip_main_t *ph, double w_u, double w_b)
        : phil::ilp_converter_t(ph),
          m_weight_unification(w_u), m_weight_backchain(w_b) {}
    
    virtual phil::ilp_converter_t* duplicate(phil::phillip_main_t *ph) const override
        {
            /* GENERATES A COPY INSTANCE. */
            return new my_ilp_converter_t(
                ph, m_weight_unification, m_weight_backchain);
        }
    
    virtual phil::ilp::ilp_problem_t* execute() const override
        {
            /* THE FOLLOWING PROCEDURE IS COMMON CONVERSION.
               HERE, THE STRUCTURE OF POTENTIAL ELEMENTAL HYPOTHESES
               IS CONVERTED INTO ILP-PROBLEM.  */
            
            const phil::pg::proof_graph_t *graph = phillip()->get_latent_hypotheses_set();
            phil::ilp::ilp_problem_t *prob = new phil::ilp::ilp_problem_t(
                graph, new phil::ilp::basic_solution_interpreter_t(), false);

            convert_proof_graph(prob);
            if (prob->is_timeout()) return prob;


            /* THE FOLLOWING PROCEDURE DEFINES THE EVALUATION FUNCTION,
               AS THE OBJECTIVE FUNTION IN ILP-PROBLEM. */

            for (phil::pg::edge_idx_t i = 0; i < graph->edges().size(); ++i)
            {
                phil::ilp::variable_idx_t v = prob->find_variable_with_edge(i);
                if (v < 0) continue;

                if (graph->edge(i).is_chain_edge())
                    prob->variable(v).set_coefficient(m_weight_backchain);
                if (graph->edge(i).is_unify_edge())
                    prob->variable(v).set_coefficient(m_weight_unification);
            }

            return prob;
        }
    
    virtual bool is_available(std::list<std::string> *disp) const override
        {
            /* THE CONDITIONS TO BE SATISFIED ON USING YOUR CONVERTER
             * MAY BE WRITTEN HERE. */
            if (m_weight_unification >= 0.0 and m_weight_backchain >= 0.0)
                return true;
            else
            {
                /* DEFINE ERROR MESSAGE HERE. */
                *disp = std::list<std::string>(
                    {"Some weights have invalid value.",
                     "Each weight must not be a negative number."});
                return false;
            }
        }
    virtual std::string repr() const override
        {
            /* DEFINE THE NAME OF YOUR CONVERTER HERE.
             * THE NAME IS PRINTED IN XML OUTPUT. */
            return phil::format(
                "MyILPConverter(w_u=%lf,w_b=%lf)",
                m_weight_unification, m_weight_backchain);
        }

private:
    double m_weight_unification;
    double m_weight_backchain;
};


/** A generator of your ilp-convertetr. */
class my_ilp_converter_generator_t :
    public phil::bin::component_generator_t<phil::ilp_converter_t>
{
public:
    /** Generates an instance your converter and returns it. */
    virtual phil::ilp_converter_t* operator()(phil::phillip_main_t *ph) const override
        {
            /* YOU CAN PROVIDE THE WAY TO CUSTOMIZE YOUR COMPONENT
               BY USING PHILLIP'S PARAMETERS OR FLAGS. */

            /* THE VALUE OF EACH WEIGHT ON DEFAULT IS 1.0. */
            double w_u = static_cast<double>(ph->param_float("my_ilp_w_u", 1.0));
            double w_b = static_cast<double>(ph->param_float("my_ilp_w_b", 1.0));
            
            return new my_ilp_converter_t(ph, w_u, w_b);
        }
};


int main(int argc, char* argv[])
{
    using namespace phil;

    /* WHAT YOU HAVE TO ADD TO main() IS ONLY THIS.
     * THIS LINE ALLOWS YOU TO USE YOUR CONVERTER
     * THROUGH THE "-c ilp=mine" OPTION IN COMMAND OPTIONS. */
    bin::ilp_converter_library_t::instance()->add(
        "mine", new my_ilp_converter_generator_t());

    
    /* THE FOLLOWING CODES ARE JUST COPIED FROM main() IN src/main.cpp. */

    phillip_main_t phillip;
    bin::execution_configure_t config;
    bin::inputs_t inputs;

    bin::prepare(argc, argv, &phillip, &config, &inputs);
    bin::execute(&phillip, config, inputs);
}


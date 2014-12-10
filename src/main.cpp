/* -*- coding: utf-8 -*- */


#include "./binary.h"
#include "./processor.h"


/** The main function.
 *  Observations is read from stdin or text file. */
int main(int argc, char* argv[])
{
    using namespace phil;
    initialize();

    phillip_main_t phillip;
    bin::execution_configure_t config;
    std::vector<std::string> inputs;

    print_console("Phillip starts...");
    print_console("  version: " + phillip_main_t::VERSION);
    
    bin::parse_options(argc, argv, &phillip, &config, &inputs);
    print_console("Phillip has completed parsing comand options.");

    bin::preprocess(config, &phillip);

    bool do_compile =
        (config.mode == bin::EXE_MODE_COMPILE_KB) or
        phillip.flag("do_compile_kb");

    /* COMPILING KNOWLEDGE-BASE */
    if (do_compile)
    {
        kb::knowledge_base_t *kb = kb::knowledge_base_t::instance();
        proc::processor_t processor;
        print_console("Compiling knowledge-base ...");

        kb->prepare_compile();

        processor.add_component(new proc::compile_kb_t());
        processor.process(inputs);

        kb->finalize();

        print_console("Completed to compile knowledge-base.");
    }

    /* INFERENCE */
    if (config.mode == bin::EXE_MODE_INFERENCE)
    {
        std::vector<lf::input_t> parsed_inputs;
        proc::processor_t processor;
        bool do_parallel_inference(phillip.flag("do_parallel_inference"));
        bool do_write_parallel_out(phillip.flag("do_write_parallel_out"));
        bool flag_printing(false);

        print_console("Loading observations ...");

        processor.add_component(new proc::parse_obs_t(&parsed_inputs));
        processor.process(inputs);

        print_console("Completed to load observations.");
        print_console_fmt("    # of observations: %d", parsed_inputs.size());

        kb::knowledge_base_t::instance()->prepare_query();

        if (kb::knowledge_base_t::instance()->is_valid_version())
        for (int i = 0; i < parsed_inputs.size(); ++i)
        {
            const lf::input_t &ipt = parsed_inputs.at(i);
            
            std::string obs_name = ipt.name;
            if (obs_name.rfind("::") != std::string::npos)
                obs_name = obs_name.substr(obs_name.rfind("::") + 2);
            
            if (phillip.is_target(obs_name) and not phillip.is_excluded(obs_name))
            {
                print_console_fmt("Observation #%d: %s", i, ipt.name.c_str());
                kb::knowledge_base_t::instance()->clear_distance_cache();
                do_parallel_inference ?
                    phillip.infer_parallel(parsed_inputs, i, do_write_parallel_out) :
                    phillip.infer(parsed_inputs, i);

                if (not flag_printing)
                {
                    phillip.write_header();
                    flag_printing = true;
                }

                auto sols = phillip.get_solutions();
                for (auto sol = sols.begin(); sol != sols.end(); ++sol)
                    sol->print_graph();
            }
        }

        if (flag_printing)
            phillip.write_footer();
    }
}

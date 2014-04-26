/* -*- coding: utf-8 -*- */


#include "./binary.h"
#include "./processor.h"


/** The main function.
 *  Observations is read from stdin or text file. */
int main(int argc, char* argv[])
{
    using namespace phil;
    initialize();

    std::vector<std::string> inputs;
    bin::execution_configure_t config;

    print_console("Phillip starts...");
    
    bin::parse_options(argc, argv, &config, &inputs);
    print_console("Phillip has completed parsing comand options.");

    bin::preprocess(&config);

    bool do_compile =
        (config.mode == bin::EXE_MODE_COMPILE_KB) or
        sys()->flag("do_compile_kb");

    /* COMPILING KNOWLEDGE-BASE */
    if (do_compile)
    {
        kb::knowledge_base_t *kb = sys()->knowledge_base();
        proc::processor_t processor;
        print_console("Compiling knowledge-base ...");

        kb->prepare_compile();

        processor.add_component(new proc::compile_kb_t(kb));
        processor.process(inputs);

        kb->finalize();

        print_console("Completed to compile knowledge-base.");
    }

    /* INFERENCE */
    if ( config.mode == bin::EXE_MODE_INFERENCE )
    {
        std::vector<lf::input_t> parsed_inputs;
        proc::processor_t processor;

        print_console("Loading observations ...");

        processor.add_component(new proc::parse_obs_t(&parsed_inputs));
        processor.process(inputs);

        print_console("Completed to load observations.");
        print_console_fmt("    # of observations: %d", parsed_inputs.size());

        sys()->knowledge_base()->prepare_query();

        for (int i = 0; i < parsed_inputs.size(); ++i)
        {
            const lf::input_t &ipt = parsed_inputs.at(i);
            print_console_fmt("Observation #%d: %s", i, ipt.name.c_str());
            sys()->infer(parsed_inputs, i);
        }

        sys()->knowledge_base()->finalize();
    }
}

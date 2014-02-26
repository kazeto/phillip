/* -*- coding: utf-8 -*- */


#include "./binary.h"
#include "./processor.h"


/**
 * The main function.
 * Observations is read from stdin or text file.
 */
int main( int argc, char* argv[] )
{
    using namespace henry;
    
    std::vector<std::string>   inputs;
    bin::execution_configure_t config;

    std::cerr << time_stamp() << "Phillip starts..." << std::endl;
    
    bin::parse_options( argc, argv, &config, &inputs );
    std::cerr << time_stamp()
              << "Parsing comand options has completed." << std::endl;

    bin::preprocess( &config );

    bool do_compile =
        (config.mode == bin::EXE_MODE_COMPILE_KB) or
        (sys()->flag("do_compile_kb"));

    /* COMPILING KNOWLEDGE-BASE */
    if (do_compile)
    {
        proc::processor_t processor;
        std::cerr << time_stamp()
            << "Compiling knowledge-base ..." << std::endl;

        config.kb->prepare_compile();
        processor.add_component( new proc::compile_kb_t(config.kb) );
        processor.process(inputs);

        std::cerr << time_stamp()
            << "Completed to compile knowledge-base." << std::endl;
    }

    /* INFERENCE */
    if ( config.mode == bin::EXE_MODE_INFERENCE )
    {
        std::vector<lf::input_t> parsed_inputs;
        proc::processor_t processor;

        std::cerr << time_stamp()
            << "Loading observations ..." << std::endl;

        processor.add_component( new proc::parse_obs_t(&parsed_inputs) );
        processor.process( inputs );

        std::cerr << time_stamp()
            << "Completed to load observations." << std::endl;

        config.kb->prepare_query();
        for( auto it=parsed_inputs.begin(); it!=parsed_inputs.end(); ++it )
            sys()->infer( *it );
    }
}

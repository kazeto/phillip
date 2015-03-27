/* -*- coding: utf-8 -*- */


#include "./binary.h"
#include "./processor.h"


/** The main function.
 *  Observations is read from stdin or text file. */
int main(int argc, char* argv[])
{
    using namespace phil;

    phillip_main_t phillip;
    bin::execution_configure_t config;
    bin::inputs_t inputs;

    try
    {
        bin::prepare(argc, argv, &phillip, &config, &inputs);
        bin::execute(&phillip, config, inputs);
    }
    catch (const phillip_exception_t &exception)
    {
        print_error(exception.what());
        if (exception.do_print_usage()) bin::print_usage();
    }

}

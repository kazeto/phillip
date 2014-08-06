#ifndef HENRY_BINARY_H
#define HENRY_BINARY_H

#include <string>
#include <vector>
#include <fstream>
#include <iostream>

#include "./phillip.h"
#include "./lhs/lhs_enumerator.h"
#include "./ilp/ilp_converter.h"
#include "./sol/ilp_solver.h"


namespace phil
{

namespace bin
{


enum execution_mode_e
{
    EXE_MODE_UNDERSPECIFIED,
    EXE_MODE_INFERENCE,
    EXE_MODE_COMPILE_KB
};


struct execution_configure_t
{
    execution_configure_t();
    
    execution_mode_e mode;
    std::string kb_name;         /// Filename of compile_kb's output.
    std::string target_obs_name; /// Name of observation to solve.

    std::string lhs_key, ilp_key, sol_key, dist_key;
};


/** Parse command line options.
 *  @param[out] option Options about binary execution.
 *  @param[out] inputs List of input filenames. */
bool parse_options(
    int argc, char* argv[],
    phillip_main_t *phillip,
    execution_configure_t *config,
    std::vector<std::string> *inputs);

/** Preprocess about execution configure.
 *  Create instance of each component of phil according to config. */
bool preprocess(const execution_configure_t &config, phillip_main_t *phillip);


extern const std::string USAGE;
extern char ACCEPTABLE_OPTIONS[];


}

}


#endif

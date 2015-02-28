#ifndef HENRY_BINARY_H
#define HENRY_BINARY_H

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <memory>

#include "./phillip.h"
#include "./lhs/lhs_enumerator.h"
#include "./ilp/ilp_converter.h"
#include "./sol/ilp_solver.h"


namespace phil
{

namespace bin
{


typedef std::vector<std::string> inputs_t;


enum execution_mode_e
{
    EXE_MODE_UNDERSPECIFIED,
    EXE_MODE_INFERENCE,
    EXE_MODE_HELP,
    EXE_MODE_COMPILE_KB
};


struct execution_configure_t
{
    execution_configure_t();
    
    execution_mode_e mode;
    std::string kb_name; /// Filename of compile_kb's output.
    hash_set<std::string> target_obs_names; /// Name of observation to solve.
    hash_set<std::string> excluded_obs_names; /// Name of observation to solve.

    std::string lhs_key, ilp_key, sol_key, dist_key;
};


template <class T> class component_generator_t
{
public:
    virtual T* operator()(phillip_main_t*) const { return NULL; }
};


template <class T> class component_library_t
: protected hash_map<std::string, component_generator_t<T>*>
{
public:
    virtual ~component_library_t()
    {
        for (auto it = begin(); it != end(); ++it)
            delete it->second;
    }

    void add(const std::string &key, component_generator_t<T> *ptr)
    {
        insert(std::make_pair(key, ptr));
    }

    T* generate(const std::string &key, phillip_main_t *ph) const
    {
        auto found = find(key);
        return (found != end()) ? (*found->second)(ph) : NULL;
    }
};


/** A class to generate lhs-enumerator from string key. */
class lhs_enumerator_library_t : public component_library_t<lhs_enumerator_t>
{
public:
    static lhs_enumerator_library_t* instance();

private:
    lhs_enumerator_library_t();

    static std::unique_ptr<
        lhs_enumerator_library_t,
        deleter_t<lhs_enumerator_library_t> > ms_instance;
};


/** A class to generate ilp-converter from string key. */
class ilp_converter_library_t : public component_library_t<ilp_converter_t>
{
public:
    static ilp_converter_library_t* instance();

private:
    ilp_converter_library_t();

    static std::unique_ptr<
        ilp_converter_library_t,
        deleter_t<ilp_converter_library_t> > ms_instance;
};


/** A class to generate ilp-solver from string key. */
class ilp_solver_library_t : public component_library_t<ilp_solver_t>
{
public:
    static ilp_solver_library_t* instance();

private:
    ilp_solver_library_t();

    static std::unique_ptr<
        ilp_solver_library_t,
        deleter_t<ilp_solver_library_t> > ms_instance;
};


/** A class to generate distance-provider from string key. */
class distance_provider_library_t : public component_library_t<kb::distance_provider_t>
{
public:
    static distance_provider_library_t* instance();

private:
    distance_provider_library_t();

    static std::unique_ptr<
        distance_provider_library_t,
        deleter_t<distance_provider_library_t> > ms_instance;
};



/** The preprocess of inference or compiling.
 *  This should be called before calling bin::execute. */
void prepare(
    int argc, char* argv[], phillip_main_t *phillip,
    execution_configure_t *config, inputs_t *inputs);


/** The main process, which performs inference or compiling. */
void execute(
    phillip_main_t *phillip,
    const execution_configure_t &config, const inputs_t &inputs);


/** The sub-routine of bin::prepare, which parses command line options.
 *  @param[out] option Options about binary execution.
 *  @param[out] inputs List of input filenames. */
bool parse_options(
    int argc, char* argv[],
    phillip_main_t *phillip,
    execution_configure_t *config, inputs_t *inputs);

/** The sub-routine of bin::prepare.
 *  Creates instance of each component of phil according to config. */
bool preprocess(const execution_configure_t &config, phillip_main_t *phillip);

/** Prints simple usage to stderr. */
void print_usage();


}

}


#endif
